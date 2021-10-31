#include "../../edition/voxel_tool.h"
#include "../../util/funcs.h"
#include "../../util/godot/funcs.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../voxel_lod_terrain.h"
#include "voxel_instance_component.h"
#include "voxel_instance_library_scene_item.h"
#include "voxel_instancer_rigidbody.h"

#include <scene/3d/camera.h>
#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/main/viewport.h>
#include <algorithm>

VoxelInstancer::VoxelInstancer() {
	set_notify_transform(true);
	set_process_internal(true);
}

VoxelInstancer::~VoxelInstancer() {
	// Destroy everything
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		memdelete(*it);
		// We don't destroy nodes, we assume they were detached already
	}
	if (_library.is_valid()) {
		_library->remove_listener(this);
	}
}

void VoxelInstancer::clear_blocks() {
	VOXEL_PROFILE_SCOPE();
	// Destroy blocks, keep configured layers
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		for (int i = 0; i < block->bodies.size(); ++i) {
			VoxelInstancerRigidBody *body = block->bodies[i];
			body->detach_and_destroy();
		}
		for (int i = 0; i < block->scene_instances.size(); ++i) {
			SceneInstance instance = block->scene_instances[i];
			ERR_CONTINUE(instance.component == nullptr);
			instance.component->detach();
			ERR_CONTINUE(instance.root == nullptr);
			instance.root->queue_delete();
		}
		memdelete(block);
	}
	_blocks.clear();
	const int *key = nullptr;
	while ((key = _layers.next(key))) {
		Layer *layer = get_layer(*key);
		layer->blocks.clear();
	}
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		lod.modified_blocks.clear();
	}
}

void VoxelInstancer::clear_blocks_in_layer(int layer_id) {
	// Not optimal, but should work for now
	for (size_t i = 0; i < _blocks.size(); ++i) {
		Block *block = _blocks[i];
		if (block->layer_id == layer_id) {
			remove_block(i);
			// remove_block does a remove-at-swap so we have to re-iterate on the same slot
			--i;
		}
	}
}

void VoxelInstancer::clear_layers() {
	clear_blocks();
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		lod.layers.clear();
		lod.modified_blocks.clear();
	}
	_layers.clear();
}

void VoxelInstancer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD:
			set_world(*get_world());
			update_visibility();
			break;

		case NOTIFICATION_EXIT_WORLD:
			set_world(nullptr);
			break;

		case NOTIFICATION_PARENTED:
			_parent = Object::cast_to<VoxelLodTerrain>(get_parent());
			if (_parent != nullptr) {
				_parent->set_instancer(this);
			}
			// TODO may want to reload all instances? Not sure if worth implementing that use case
			break;

		case NOTIFICATION_UNPARENTED:
			clear_blocks();
			if (_parent != nullptr) {
				_parent->set_instancer(nullptr);
			}
			_parent = nullptr;
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			VOXEL_PROFILE_SCOPE_NAMED("VoxelInstancer::NOTIFICATION_TRANSFORM_CHANGED");

			if (!is_inside_tree() || _parent == nullptr) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			const Transform parent_transform = get_global_transform();
			const int base_block_size_po2 = _parent->get_mesh_block_size_pow2();
			//print_line(String("IP: {0}").format(varray(parent_transform.origin)));

			for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
				Block *block = *it;
				if (!block->multimesh_instance.is_valid()) {
					// The block exists as an empty block (if it did not exist, it would get generated)
					continue;
				}
				const int block_size_po2 = base_block_size_po2 + block->lod_index;
				const Vector3 block_local_pos = (block->grid_position << block_size_po2).to_vec3();
				// The local block transform never has rotation or scale so we can take a shortcut
				const Transform block_transform(parent_transform.basis, parent_transform.xform(block_local_pos));
				block->multimesh_instance.set_transform(block_transform);
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			update_visibility();
			break;

		case NOTIFICATION_INTERNAL_PROCESS:
			if (_parent != nullptr && _library.is_valid()) {
				process_mesh_lods();
			}
			break;
	}
}

VoxelInstancer::Layer *VoxelInstancer::get_layer(int id) {
	Layer *ptr = _layers.getptr(id);
	CRASH_COND(ptr == nullptr);
	return ptr;
}

const VoxelInstancer::Layer *VoxelInstancer::get_layer_const(int id) const {
	const Layer *ptr = _layers.getptr(id);
	CRASH_COND(ptr == nullptr);
	return ptr;
}

void VoxelInstancer::process_mesh_lods() {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	// Get viewer position
	const Viewport *viewport = get_viewport();
	const Camera *camera = viewport->get_camera();
	if (camera == nullptr) {
		return;
	}
	const Transform gtrans = get_global_transform();
	const Vector3 cam_pos_local = (gtrans.affine_inverse() * camera->get_global_transform()).origin;

	ERR_FAIL_COND(_parent == nullptr);
	const int block_size = _parent->get_mesh_block_size();

	// Hardcoded LOD thresholds for now.
	// Can't really use pixel density because view distances are controlled by the main surface LOD octree
	{
		const int block_region_extent = _parent->get_mesh_block_region_extent();

		FixedArray<float, 4> coeffs;
		coeffs[0] = 0;
		coeffs[1] = 0.1;
		coeffs[2] = 0.25;
		coeffs[3] = 0.5;
		const float hysteresis = 1.05;

		for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
			Lod &lod = _lods[lod_index];
			const float max_distance = block_size * (block_region_extent << lod_index);

			for (unsigned int j = 0; j < lod.mesh_lod_distances.size(); ++j) {
				MeshLodDistances &mld = lod.mesh_lod_distances[j];
				mld.exit_distance_squared = max_distance * max_distance * coeffs[j];
				mld.enter_distance_squared = hysteresis * mld.exit_distance_squared;
			}
		}
	}

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;

		const VoxelInstanceLibraryItemBase *item_base = _library->get_item_const(block->layer_id);
		ERR_CONTINUE(item_base == nullptr);
		// TODO Optimization: would be nice to not need this cast by iterating only the same item types
		const VoxelInstanceLibraryItem *item = Object::cast_to<VoxelInstanceLibraryItem>(item_base);
		if (item == nullptr) {
			// Not a multimesh item
			continue;
		}
		const int mesh_lod_count = item->get_mesh_lod_count();
		if (mesh_lod_count <= 1) {
			// This block has no LOD
			// TODO Optimization: would be nice to not need this conditional by iterating only item types that define lods
			continue;
		}

		const int lod_index = item->get_lod_index();

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(mesh_lod_count < VoxelInstanceLibraryItem::MAX_MESH_LODS);
#endif

		const Lod &lod = _lods[lod_index];

		const int lod_block_size = block_size << lod_index;
		const int hs = lod_block_size >> 1;
		const Vector3 block_center_local = (block->grid_position * lod_block_size + Vector3i(hs, hs, hs)).to_vec3();
		const float distance_squared = cam_pos_local.distance_squared_to(block_center_local);

		if (block->current_mesh_lod + 1 < mesh_lod_count &&
				distance_squared > lod.mesh_lod_distances[block->current_mesh_lod].enter_distance_squared) {
			// Decrease detail
			++block->current_mesh_lod;
			Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
			if (multimesh.is_valid()) {
				multimesh->set_mesh(item->get_mesh(block->current_mesh_lod));
			}
		}

		if (block->current_mesh_lod > 0 &&
				distance_squared < lod.mesh_lod_distances[block->current_mesh_lod].exit_distance_squared) {
			// Increase detail
			--block->current_mesh_lod;
			Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
			if (multimesh.is_valid()) {
				multimesh->set_mesh(item->get_mesh(block->current_mesh_lod));
			}
		}
	}
}

// We need to do this ourselves because we don't use nodes for multimeshes
void VoxelInstancer::update_visibility() {
	if (!is_inside_tree()) {
		return;
	}
	const bool visible = is_visible_in_tree();
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		if (block->multimesh_instance.is_valid()) {
			block->multimesh_instance.set_visible(visible);
		}
	}
}

void VoxelInstancer::set_world(World *world) {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		if (block->multimesh_instance.is_valid()) {
			block->multimesh_instance.set_world(world);
		}
	}
}

void VoxelInstancer::set_up_mode(UpMode mode) {
	ERR_FAIL_COND(mode < 0 || mode >= UP_MODE_COUNT);
	if (_up_mode == mode) {
		return;
	}
	_up_mode = mode;
	const int *key = nullptr;
	while ((key = _layers.next(key))) {
		regenerate_layer(*key, false);
	}
}

VoxelInstancer::UpMode VoxelInstancer::get_up_mode() const {
	return _up_mode;
}

void VoxelInstancer::set_library(Ref<VoxelInstanceLibrary> library) {
	if (library == _library) {
		return;
	}

	if (_library.is_valid()) {
		_library->remove_listener(this);
	}

	_library = library;

	clear_layers();

	if (_library.is_valid()) {
		_library->for_each_item([this](int id, const VoxelInstanceLibraryItemBase &item) {
			add_layer(id, item.get_lod_index());
			if (_parent != nullptr && is_inside_tree()) {
				regenerate_layer(id, true);
			}
		});

		_library->add_listener(this);
	}

	update_configuration_warning();
}

Ref<VoxelInstanceLibrary> VoxelInstancer::get_library() const {
	return _library;
}

void VoxelInstancer::regenerate_layer(uint16_t layer_id, bool regenerate_blocks) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);

	Ref<World> world_ref = get_world();
	ERR_FAIL_COND(world_ref.is_null());
	World *world = *world_ref;

	Layer *layer = get_layer(layer_id);
	CRASH_COND(layer == nullptr);

	Ref<VoxelInstanceLibraryItemBase> item = _library->get_item(layer_id);
	ERR_FAIL_COND(item.is_null());
	if (item->get_generator().is_null()) {
		return;
	}

	const Transform parent_transform = get_global_transform();

	if (regenerate_blocks) {
		// Create blocks
		Vector<Vector3i> positions = _parent->get_meshed_block_positions_at_lod(layer->lod_index);
		for (int i = 0; i < positions.size(); ++i) {
			const Vector3i pos = positions[i];

			const unsigned int *iptr = layer->blocks.getptr(pos);
			if (iptr != nullptr) {
				continue;
			}

			create_block(layer, layer_id, pos);
		}
	}

	const int render_to_data_factor = _parent->get_mesh_block_size() / _parent->get_data_block_size();
	ERR_FAIL_COND(render_to_data_factor <= 0 || render_to_data_factor > 2);

	struct L {
		// Does not return a bool so it can be used in bit-shifting operations without a compiler warning.
		// Can be treated like a bool too.
		static inline uint8_t has_edited_block(const Lod &lod, Vector3i pos) {
			return lod.modified_blocks.has(pos) ||
				   lod.loaded_instances_data.find(pos) != lod.loaded_instances_data.end();
		}

		static inline void extract_octant_transforms(
				const Block &render_block, std::vector<Transform> &dst, uint8_t octant_mask, int render_block_size) {
			if (!render_block.multimesh_instance.is_valid()) {
				return;
			}
			Ref<MultiMesh> multimesh = render_block.multimesh_instance.get_multimesh();
			ERR_FAIL_COND(multimesh.is_null());
			const int instance_count = get_visible_instance_count(**multimesh);
			const float h = render_block_size / 2;
			for (int i = 0; i < instance_count; ++i) {
				// TODO This is terrible in MT mode! Think about keeping a local copy...
				const Transform t = multimesh->get_instance_transform(i);
				const uint8_t octant_index = VoxelInstanceGenerator::get_octant_index(t.origin, h);
				if ((octant_mask & (1 << octant_index)) != 0) {
					dst.push_back(t);
				}
			}
		}
	};

	// Update existing blocks
	for (size_t block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block *block = _blocks[block_index];
		CRASH_COND(block == nullptr);
		if (block->layer_id != layer_id) {
			continue;
		}
		const int lod_index = block->lod_index;
		const Lod &lod = _lods[lod_index];

		uint8_t octant_mask = 0xff;
		if (render_to_data_factor == 1) {
			if (L::has_edited_block(lod, block->grid_position)) {
				// Was edited, no regen on this
				continue;
			}
		} else if (render_to_data_factor == 2) {
			// The rendering block corresponds to 8 smaller data blocks
			octant_mask = 0;
			const Vector3i data_pos0 = block->grid_position * render_to_data_factor;
			octant_mask |= L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y, data_pos0.z));
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y, data_pos0.z)) << 1);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y + 1, data_pos0.z)) << 2);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y + 1, data_pos0.z)) << 3);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y, data_pos0.z + 1)) << 4);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y, data_pos0.z + 1)) << 5);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y + 1, data_pos0.z + 1)) << 6);
			octant_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y + 1, data_pos0.z + 1)) << 7);
			if (octant_mask == 0) {
				// All data blocks were edited, no regen on the whole render block
				continue;
			}
		}

		_transform_cache.clear();

		Array surface_arrays = _parent->get_mesh_block_surface(block->grid_position, lod_index);

		const int lod_block_size = _parent->get_mesh_block_size() << lod_index;
		const Transform block_local_transform =
				Transform(Basis(), (block->grid_position * lod_block_size).to_vec3());
		const Transform block_transform = parent_transform * block_local_transform;

		item->get_generator()->generate_transforms(
				_transform_cache,
				block->grid_position,
				block->lod_index,
				layer_id,
				surface_arrays,
				block_local_transform,
				static_cast<VoxelInstanceGenerator::UpMode>(_up_mode),
				octant_mask,
				lod_block_size);

		if (render_to_data_factor == 2 && octant_mask != 0xff) {
			// Complete transforms with edited ones
			L::extract_octant_transforms(*block, _transform_cache, ~octant_mask, _parent->get_mesh_block_size());
			// TODO What if these blocks had loaded data which wasn't yet uploaded for render?
			// We may setup a local transform list as well since it's expensive to get it from VisualServer
		}

		update_block_from_transforms(block_index, to_span_const(_transform_cache),
				block->grid_position, layer, *item, layer_id, world, block_transform);
	}
}

void VoxelInstancer::update_layer_meshes(int layer_id) {
	Ref<VoxelInstanceLibraryItemBase> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	VoxelInstanceLibraryItem *item = Object::cast_to<VoxelInstanceLibraryItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		if (block->layer_id != layer_id || !block->multimesh_instance.is_valid()) {
			continue;
		}
		block->multimesh_instance.set_material_override(item->get_material_override());
		block->multimesh_instance.set_cast_shadows_setting(item->get_cast_shadows_setting());
		Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
		if (multimesh.is_valid()) {
			Ref<Mesh> mesh;
			if (item->get_mesh_lod_count() <= 1) {
				mesh = item->get_mesh(0);
			} else {
				mesh = item->get_mesh(block->current_mesh_lod);
			}
			multimesh->set_mesh(mesh);
		}
	}
}

void VoxelInstancer::update_layer_scenes(int layer_id) {
	Ref<VoxelInstanceLibraryItemBase> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	VoxelInstanceLibrarySceneItem *item = Object::cast_to<VoxelInstanceLibrarySceneItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);
	const int data_block_size_po2 = _parent->get_data_block_size_pow2();

	for (unsigned int block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block *block = _blocks[block_index];
		ERR_CONTINUE(block == nullptr);

		for (int instance_index = 0; instance_index < block->scene_instances.size(); ++instance_index) {
			SceneInstance prev_instance = block->scene_instances[instance_index];
			ERR_CONTINUE(prev_instance.root == nullptr);
			SceneInstance instance = create_scene_instance(
					*item, instance_index, block_index, prev_instance.root->get_transform(), data_block_size_po2);
			ERR_CONTINUE(instance.root == nullptr);
			block->scene_instances.write[instance_index] = instance;
			// We just drop the instance without saving, because this function is supposed to occur only in editor,
			// or in the very rare cases where library is modified in game (which would invalidate saves anyways).
			prev_instance.root->queue_delete();
		}
	}
}

void VoxelInstancer::on_library_item_changed(int item_id, VoxelInstanceLibraryItemBase::ChangeType change) {
	ERR_FAIL_COND(_library.is_null());

	// TODO It's unclear yet if some code paths do the right thing in case instances got edited

	switch (change) {
		case VoxelInstanceLibraryItemBase::CHANGE_ADDED: {
			Ref<VoxelInstanceLibraryItemBase> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());
			add_layer(item_id, item->get_lod_index());
			regenerate_layer(item_id, true);
			update_configuration_warning();
		} break;

		case VoxelInstanceLibraryItemBase::CHANGE_REMOVED:
			remove_layer(item_id);
			update_configuration_warning();
			break;

		case VoxelInstanceLibraryItemBase::CHANGE_GENERATOR:
			regenerate_layer(item_id, false);
			break;

		case VoxelInstanceLibraryItemBase::CHANGE_VISUAL:
			update_layer_meshes(item_id);
			break;

		case VoxelInstanceLibraryItemBase::CHANGE_SCENE:
			update_layer_scenes(item_id);
			break;

		case VoxelInstanceLibraryItemBase::CHANGE_LOD_INDEX: {
			Ref<VoxelInstanceLibraryItemBase> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());

			clear_blocks_in_layer(item_id);

			Layer *layer = get_layer(item_id);
			CRASH_COND(layer == nullptr);

			Lod &prev_lod = _lods[layer->lod_index];
			unordered_remove_value(prev_lod.layers, item_id);

			layer->lod_index = item->get_lod_index();

			Lod &new_lod = _lods[layer->lod_index];
			new_lod.layers.push_back(item_id);

			regenerate_layer(item_id, true);
		} break;

		default:
			ERR_PRINT("Unknown change");
			break;
	}
}

void VoxelInstancer::add_layer(int layer_id, int lod_index) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(lod_index < 0 || lod_index >= MAX_LOD);
	ERR_FAIL_COND(_layers.has(layer_id));
#endif

	Lod &lod = _lods[lod_index];

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(std::find(lod.layers.begin(), lod.layers.end(), layer_id) != lod.layers.end(),
			"Layer already referenced by this LOD");
#endif

	Layer layer;
	layer.lod_index = lod_index;
	_layers.set(layer_id, layer);

	lod.layers.push_back(layer_id);
}

void VoxelInstancer::remove_layer(int layer_id) {
	Layer *layer = get_layer(layer_id);

	Lod &lod = _lods[layer->lod_index];
	for (size_t i = 0; i < lod.layers.size(); ++i) {
		if (lod.layers[i] == layer_id) {
			lod.layers[i] = lod.layers.back();
			lod.layers.pop_back();
			break;
		}
	}

	Vector<int> to_remove;
	const Vector3i *block_pos_ptr = nullptr;
	while ((block_pos_ptr = layer->blocks.next(block_pos_ptr))) {
		int block_index = layer->blocks.get(*block_pos_ptr);
		to_remove.push_back(block_index);
	}

	for (int i = 0; i < to_remove.size(); ++i) {
		int block_index = to_remove[i];
		remove_block(block_index);
	}

	_layers.erase(layer_id);
}

void VoxelInstancer::remove_block(unsigned int block_index) {
#ifdef DEBUG_ENABLED
	CRASH_COND(block_index >= _blocks.size());
#endif
	Block *moved_block = _blocks.back();
	Block *block = _blocks[block_index];
	{
		Layer *layer = get_layer(block->layer_id);
		layer->blocks.erase(block->grid_position);
	}
	_blocks[block_index] = moved_block;
	_blocks.pop_back();

	for (int i = 0; i < block->bodies.size(); ++i) {
		VoxelInstancerRigidBody *body = block->bodies[i];
		body->detach_and_destroy();
	}

	for (int i = 0; i < block->scene_instances.size(); ++i) {
		SceneInstance instance = block->scene_instances[i];
		ERR_CONTINUE(instance.component == nullptr);
		instance.component->detach();
		ERR_CONTINUE(instance.root == nullptr);
		instance.root->queue_delete();
	}

	memdelete(block);

	if (block != moved_block) {
		Layer *layer = get_layer(moved_block->layer_id);
		unsigned int *iptr = layer->blocks.getptr(moved_block->grid_position);
		CRASH_COND(iptr == nullptr);
		*iptr = block_index;
	}
}

void VoxelInstancer::on_data_block_loaded(Vector3i grid_position, unsigned int lod_index,
		std::unique_ptr<VoxelInstanceBlockData> instances) {
	ERR_FAIL_COND(lod_index >= _lods.size());
	Lod &lod = _lods[lod_index];
	lod.loaded_instances_data.insert(std::make_pair(grid_position, std::move(instances)));
}

void VoxelInstancer::on_mesh_block_enter(Vector3i render_grid_position, unsigned int lod_index, Array surface_arrays) {
	if (lod_index >= _lods.size()) {
		return;
	}
	create_render_blocks(render_grid_position, lod_index, surface_arrays);
}

void VoxelInstancer::on_mesh_block_exit(Vector3i render_grid_position, unsigned int lod_index) {
	if (lod_index >= _lods.size()) {
		return;
	}

	VOXEL_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];

	// Remove data blocks
	const int render_to_data_factor = _parent->get_mesh_block_size() / _parent->get_data_block_size();
	const Vector3i data_min_pos = render_grid_position * render_to_data_factor;
	const Vector3i data_max_pos = data_min_pos + Vector3i(render_to_data_factor);
	Vector3i data_grid_pos;
	for (data_grid_pos.z = data_min_pos.z; data_grid_pos.z < data_max_pos.z; ++data_grid_pos.z) {
		for (data_grid_pos.y = data_min_pos.y; data_grid_pos.y < data_max_pos.y; ++data_grid_pos.y) {
			for (data_grid_pos.x = data_min_pos.x; data_grid_pos.x < data_max_pos.x; ++data_grid_pos.x) {
				// If we loaded data there but it was never used, we'll unload it either way
				lod.loaded_instances_data.erase(data_grid_pos);

				if (lod.modified_blocks.has(data_grid_pos)) {
					save_block(data_grid_pos, lod_index);
					lod.modified_blocks.erase(data_grid_pos);
				}
			}
		}
	}

	// Remove render blocks
	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_id = *it;

		Layer *layer = get_layer(layer_id);
		CRASH_COND(layer == nullptr);

		const unsigned int *block_index_ptr = layer->blocks.getptr(render_grid_position);
		if (block_index_ptr != nullptr) {
			remove_block(*block_index_ptr);
		}
	}
}

void VoxelInstancer::save_all_modified_blocks() {
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		const Vector3i *key = nullptr;
		while ((key = lod.modified_blocks.next(key))) {
			save_block(*key, lod_index);
		}
		lod.modified_blocks.clear();
	}
}

VoxelInstancer::SceneInstance VoxelInstancer::create_scene_instance(const VoxelInstanceLibrarySceneItem &scene_item,
		int instance_index, unsigned int block_index, Transform transform, int data_block_size_po2) {
	SceneInstance instance;
	ERR_FAIL_COND_V(scene_item.get_scene().is_null(), instance);
	Node *root = scene_item.get_scene()->instance();
	ERR_FAIL_COND_V(root == nullptr, instance);
	instance.root = Object::cast_to<Spatial>(root);
	ERR_FAIL_COND_V_MSG(instance.root == nullptr, instance, "Root of scene instance must be derived from Spatial");

	instance.component = VoxelInstanceComponent::find_in(instance.root);
	if (instance.component == nullptr) {
		instance.component = memnew(VoxelInstanceComponent);
		instance.root->add_child(instance.component);
	}

	instance.component->attach(this);
	instance.component->set_instance_index(instance_index);
	instance.component->set_render_block_index(block_index);
	instance.component->set_data_block_position(Vector3i::from_floored(transform.origin) >> data_block_size_po2);

	instance.root->set_transform(transform);

	// This is the SLOWEST part
	add_child(instance.root);

	return instance;
}

int VoxelInstancer::create_block(Layer *layer, uint16_t layer_id, Vector3i grid_position) {
	Block *block = memnew(Block);
	block->layer_id = layer_id;
	block->current_mesh_lod = 0;
	block->lod_index = layer->lod_index;
	block->grid_position = grid_position;
	int block_index = _blocks.size();
	_blocks.push_back(block);
#ifdef DEBUG_ENABLED
	CRASH_COND(layer->blocks.has(grid_position));
#endif
	layer->blocks.set(grid_position, block_index);
	return block_index;
}

void VoxelInstancer::update_block_from_transforms(int block_index, Span<const Transform> transforms,
		Vector3i grid_position, Layer *layer, const VoxelInstanceLibraryItemBase *item_base, uint16_t layer_id,
		World *world, const Transform &block_transform) {
	VOXEL_PROFILE_SCOPE();

	CRASH_COND(layer == nullptr);
	CRASH_COND(item_base == nullptr);

	// Get or create block
	Block *block = nullptr;
	if (block_index != -1) {
		block = _blocks[block_index];
	} else {
		block_index = create_block(layer, layer_id, grid_position);
		block = _blocks[block_index];
	}

	// Update multimesh
	const VoxelInstanceLibraryItem *item = Object::cast_to<VoxelInstanceLibraryItem>(item_base);
	if (item != nullptr) {
		if (transforms.size() == 0) {
			if (block->multimesh_instance.is_valid()) {
				block->multimesh_instance.set_multimesh(Ref<MultiMesh>());
				block->multimesh_instance.destroy();
			}

		} else {
			Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
			if (multimesh.is_null()) {
				multimesh.instance();
				multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
				multimesh->set_color_format(MultiMesh::COLOR_NONE);
				multimesh->set_custom_data_format(MultiMesh::CUSTOM_DATA_NONE);
			} else {
				multimesh->set_visible_instance_count(-1);
			}
			PoolRealArray bulk_array;
			DirectMultiMeshInstance::make_transform_3d_bulk_array(transforms, bulk_array);
			multimesh->set_instance_count(transforms.size());
			multimesh->set_as_bulk_array(bulk_array);

			if (item->get_mesh_lod_count() > 0) {
				multimesh->set_mesh(item->get_mesh(item->get_mesh_lod_count() - 1));
			}

			if (!block->multimesh_instance.is_valid()) {
				block->multimesh_instance.create();
				block->multimesh_instance.set_visible(is_visible());
			}
			block->multimesh_instance.set_multimesh(multimesh);
			block->multimesh_instance.set_world(world);
			block->multimesh_instance.set_transform(block_transform);
			block->multimesh_instance.set_material_override(item->get_material_override());
			block->multimesh_instance.set_cast_shadows_setting(item->get_cast_shadows_setting());
		}

		// Update bodies
		const Vector<VoxelInstanceLibraryItem::CollisionShapeInfo> &collision_shapes = item->get_collision_shapes();
		if (collision_shapes.size() > 0) {
			VOXEL_PROFILE_SCOPE_NAMED("Update multimesh bodies");

			const int data_block_size_po2 = _parent->get_data_block_size_pow2();

			// Add new bodies
			for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
				const Transform &local_transform = transforms[instance_index];
				const Transform body_transform = block_transform * local_transform;

				VoxelInstancerRigidBody *body;

				if (instance_index < static_cast<unsigned int>(block->bodies.size())) {
					body = block->bodies.write[instance_index];

				} else {
					body = memnew(VoxelInstancerRigidBody);
					body->attach(this);
					body->set_instance_index(instance_index);
					body->set_render_block_index(block_index);
					body->set_data_block_position(Vector3i::from_floored(body_transform.origin) >> data_block_size_po2);

					for (int i = 0; i < collision_shapes.size(); ++i) {
						const VoxelInstanceLibraryItem::CollisionShapeInfo &shape_info = collision_shapes[i];
						CollisionShape *cs = memnew(CollisionShape);
						cs->set_shape(shape_info.shape);
						cs->set_transform(shape_info.transform);
						body->add_child(cs);
					}

					add_child(body);
					block->bodies.push_back(body);
				}

				body->set_transform(body_transform);
			}

			// Remove old bodies
			for (int instance_index = transforms.size(); instance_index < block->bodies.size(); ++instance_index) {
				VoxelInstancerRigidBody *body = block->bodies[instance_index];
				body->detach_and_destroy();
			}

			block->bodies.resize(transforms.size());
		}
	}

	// Update scene instances
	const VoxelInstanceLibrarySceneItem *scene_item = Object::cast_to<VoxelInstanceLibrarySceneItem>(item_base);
	if (scene_item != nullptr) {
		VOXEL_PROFILE_SCOPE_NAMED("Update scene instances");
		ERR_FAIL_COND(scene_item->get_scene().is_null());

		const int data_block_size_po2 = _parent->get_data_block_size_pow2();

		// Add new instances
		for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
			const Transform &local_transform = transforms[instance_index];
			const Transform body_transform = block_transform * local_transform;

			SceneInstance instance;

			if (instance_index < static_cast<unsigned int>(block->bodies.size())) {
				instance = block->scene_instances.write[instance_index];
				instance.root->set_transform(body_transform);

			} else {
				instance = create_scene_instance(
						*scene_item, instance_index, block_index, body_transform, data_block_size_po2);
				ERR_CONTINUE(instance.root == nullptr);
				block->scene_instances.push_back(instance);
			}

			// TODO Deserialize state
		}

		// Remove old instances
		for (int instance_index = transforms.size(); instance_index < block->scene_instances.size(); ++instance_index) {
			SceneInstance instance = block->scene_instances[instance_index];
			ERR_CONTINUE(instance.component == nullptr);
			instance.component->detach();
			ERR_CONTINUE(instance.root == nullptr);
			instance.root->queue_delete();
		}

		block->scene_instances.resize(transforms.size());
	}
}

static const VoxelInstanceBlockData::LayerData *find_layer_data(const VoxelInstanceBlockData &instances_data, int id) {
	for (size_t i = 0; i < instances_data.layers.size(); ++i) {
		const VoxelInstanceBlockData::LayerData &layer = instances_data.layers[i];
		if (layer.id == id) {
			return &layer;
		}
	}
	return nullptr;
}

void VoxelInstancer::create_render_blocks(Vector3i render_grid_position, int lod_index, Array surface_arrays) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	// TODO Query one or multiple data blocks if any

	Lod &lod = _lods[lod_index];
	const Transform parent_transform = get_global_transform();
	Ref<World> world_ref = get_world();
	ERR_FAIL_COND(world_ref.is_null());
	World *world = *world_ref;

	const int lod_block_size = _parent->get_mesh_block_size() << lod_index;
	const Transform block_local_transform = Transform(Basis(), (render_grid_position * lod_block_size).to_vec3());
	const Transform block_transform = parent_transform * block_local_transform;

	const int render_to_data_factor = _parent->get_mesh_block_size() / _parent->get_data_block_size();
	const Vector3i data_min_pos = render_grid_position * render_to_data_factor;
	const Vector3i data_max_pos = data_min_pos + Vector3i(render_to_data_factor);

	const int lod_render_block_size = _parent->get_mesh_block_size() << lod_index;

	for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
		const int layer_id = *layer_it;

		Layer *layer = get_layer(layer_id);
		CRASH_COND(layer == nullptr);

		const unsigned int *render_block_index_ptr = layer->blocks.getptr(render_grid_position);
		if (render_block_index_ptr != nullptr) {
			// The block was already made?
			continue;
		}

		_transform_cache.clear();

		uint8_t gen_octant_mask = 0xff;

		// Fill in edited data
		unsigned int octant_index = 0;
		Vector3i data_grid_pos;
		for (data_grid_pos.z = data_min_pos.z; data_grid_pos.z < data_max_pos.z; ++data_grid_pos.z) {
			for (data_grid_pos.y = data_min_pos.y; data_grid_pos.y < data_max_pos.y; ++data_grid_pos.y) {
				for (data_grid_pos.x = data_min_pos.x; data_grid_pos.x < data_max_pos.x; ++data_grid_pos.x) {
					auto instances_data_it = lod.loaded_instances_data.find(data_grid_pos);

					if (instances_data_it != lod.loaded_instances_data.end()) {
						// This area has user-edited instances

						const VoxelInstanceBlockData *instances_data = instances_data_it->second.get();
						CRASH_COND(instances_data == nullptr);

						const VoxelInstanceBlockData::LayerData *layer_data =
								find_layer_data(*instances_data, layer_id);

						if (layer_data == nullptr) {
							continue;
						}

						for (auto it = layer_data->instances.begin(); it != layer_data->instances.end(); ++it) {
							const VoxelInstanceBlockData::InstanceData &d = *it;
							_transform_cache.push_back(d.transform);
						}

						// Unset bit for this octant so it won't be generated
						gen_octant_mask &= ~(1 << octant_index);
					}

					++octant_index;
				}
			}
		}

		const VoxelInstanceLibraryItemBase *item = _library->get_item(layer_id);
		CRASH_COND(item == nullptr);

		// Generate the rest
		if (gen_octant_mask != 0 && surface_arrays.size() != 0 && item->get_generator().is_valid()) {
			PoolVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];

			if (vertices.size() != 0) {
				PoolVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];
				ERR_FAIL_COND(normals.size() == 0);

				VOXEL_PROFILE_SCOPE();

				static thread_local std::vector<Transform> s_generated_transforms;
				s_generated_transforms.clear();

				item->get_generator()->generate_transforms(
						s_generated_transforms,
						render_grid_position,
						lod_index,
						layer_id,
						surface_arrays,
						block_local_transform,
						static_cast<VoxelInstanceGenerator::UpMode>(_up_mode),
						gen_octant_mask,
						lod_render_block_size);

				for (auto it = s_generated_transforms.begin(); it != s_generated_transforms.end(); ++it) {
					_transform_cache.push_back(*it);
				}
			}
		}

		update_block_from_transforms(-1, to_span_const(_transform_cache),
				render_grid_position, layer, item, layer_id, world, block_transform);
	}
}

void VoxelInstancer::save_block(Vector3i data_grid_pos, int lod_index) const {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	PRINT_VERBOSE(String("Requesting save of instance block {0} lod {1}")
						  .format(varray(data_grid_pos.to_vec3(), lod_index)));

	const Lod &lod = _lods[lod_index];

	std::unique_ptr<VoxelInstanceBlockData> data = std::make_unique<VoxelInstanceBlockData>();
	const int data_block_size = _parent->get_data_block_size() << lod_index;
	data->position_range = data_block_size;

	const int render_to_data_factor = _parent->get_mesh_block_size() / _parent->get_data_block_size();
	ERR_FAIL_COND_MSG(render_to_data_factor < 1 || render_to_data_factor > 2, "Unsupported block size");

	const int half_render_block_size = _parent->get_mesh_block_size() / 2;
	const Vector3i render_block_pos = data_grid_pos.floordiv(render_to_data_factor);

	const int octant_index = (data_grid_pos.x & 1) | ((data_grid_pos.y & 1) << 1) | ((data_grid_pos.z & 1) << 1);

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_id = *it;

		const VoxelInstanceLibraryItemBase *item = _library->get_item_const(layer_id);
		CRASH_COND(item == nullptr);
		if (!item->is_persistent()) {
			continue;
		}

		const Layer *layer = get_layer_const(layer_id);
		CRASH_COND(layer == nullptr);

		ERR_FAIL_COND(layer_id < 0);

		const unsigned int *render_block_index_ptr = layer->blocks.getptr(render_block_pos);
		if (render_block_index_ptr == nullptr) {
			continue;
		}

		const unsigned int render_block_index = *render_block_index_ptr;

#ifdef DEBUG_ENABLED
		CRASH_COND(render_block_index >= _blocks.size());
#endif
		Block *render_block = _blocks[render_block_index];
		ERR_CONTINUE(render_block == nullptr);

		data->layers.push_back(VoxelInstanceBlockData::LayerData());
		VoxelInstanceBlockData::LayerData &layer_data = data->layers.back();

		layer_data.instances.clear();
		layer_data.id = layer_id;

		if (item->get_generator().is_valid()) {
			layer_data.scale_min = item->get_generator()->get_min_scale();
			layer_data.scale_max = item->get_generator()->get_max_scale();
		} else {
			// TODO Calculate scale range automatically in the serializer
			layer_data.scale_min = 0.1f;
			layer_data.scale_max = 10.f;
		}

		if (render_block->multimesh_instance.is_valid()) {
			// Multimeshes

			Ref<MultiMesh> multimesh = render_block->multimesh_instance.get_multimesh();
			CRASH_COND(multimesh.is_null());

			VOXEL_PROFILE_SCOPE();

			const int instance_count = get_visible_instance_count(**multimesh);

			if (render_to_data_factor == 1) {
				layer_data.instances.resize(instance_count);

				// TODO Optimization: it would be nice to get the whole array at once
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO This is terrible in MT mode! Think about keeping a local copy...
					layer_data.instances[instance_index].transform =
							multimesh->get_instance_transform(instance_index);
				}

			} else if (render_to_data_factor == 2) {
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO This is terrible in MT mode! Think about keeping a local copy...
					const Transform t = multimesh->get_instance_transform(instance_index);
					const int instance_octant_index =
							VoxelInstanceGenerator::get_octant_index(t.origin, half_render_block_size);
					if (instance_octant_index == octant_index) {
						VoxelInstanceBlockData::InstanceData d;
						d.transform = t;
						layer_data.instances.push_back(d);
					}
				}
			}

		} else if (render_block->scene_instances.size() > 0) {
			// Scenes

			VOXEL_PROFILE_SCOPE();
			const unsigned int instance_count = render_block->scene_instances.size();

			if (render_to_data_factor == 1) {
				layer_data.instances.resize(instance_count);

				for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const SceneInstance instance = render_block->scene_instances[instance_index];
					ERR_CONTINUE(instance.root == nullptr);
					layer_data.instances[instance_index].transform = instance.root->get_transform();
				}

			} else if (render_to_data_factor == 2) {
				for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const SceneInstance instance = render_block->scene_instances[instance_index];
					ERR_CONTINUE(instance.root == nullptr);
					const Transform t = instance.root->get_transform();
					const int instance_octant_index =
							VoxelInstanceGenerator::get_octant_index(t.origin, half_render_block_size);
					if (instance_octant_index == octant_index) {
						VoxelInstanceBlockData::InstanceData d;
						d.transform = t;
						layer_data.instances.push_back(d);
					}
					// TODO Serialize state
				}
			}
		}
	}

	const int volume_id = _parent->get_volume_id();
	VoxelServer::get_singleton()->request_instance_block_save(volume_id, std::move(data), data_grid_pos, lod_index);
}

void VoxelInstancer::remove_floating_multimesh_instances(Block &block, const Transform &parent_transform,
		Box3i p_voxel_box, const VoxelTool &voxel_tool, int block_size_po2) {
	if (!block.multimesh_instance.is_valid()) {
		// Empty block
		return;
	}

	Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
	ERR_FAIL_COND(multimesh.is_null());

	const int initial_instance_count = get_visible_instance_count(**multimesh);
	int instance_count = initial_instance_count;

	const Transform block_global_transform = Transform(parent_transform.basis,
			parent_transform.xform((block.grid_position << block_size_po2).to_vec3()));

	// Let's check all instances one by one
	// Note: the fact we have to query VisualServer in and out is pretty bad though.
	// - We probably have to sync with its thread in MT mode
	// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
	for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
		// TODO This is terrible in MT mode! Think about keeping a local copy...
		const Transform mm_transform = multimesh->get_instance_transform(instance_index);
		const Vector3i voxel_pos(Vector3i::from_floored(mm_transform.origin + block_global_transform.origin));

		if (!p_voxel_box.contains(voxel_pos)) {
			continue;
		}

		// 1-voxel cheap check without interpolation
		const float sdf = voxel_tool.get_voxel_f(voxel_pos);
		if (sdf < -0.1f) {
			// Still enough ground
			continue;
		}

		// Remove the MultiMesh instance
		const int last_instance_index = --instance_count;
		// TODO This is terrible in MT mode! Think about keeping a local copy...
		const Transform last_trans = multimesh->get_instance_transform(last_instance_index);
		multimesh->set_instance_transform(instance_index, last_trans);

		// Remove the body if this block has some
		// TODO In the case of bodies, we could use an overlap check
		if (block.bodies.size() > 0) {
			VoxelInstancerRigidBody *rb = block.bodies[instance_index];
			// Detach so it won't try to update our instances, we already do it here
			rb->detach_and_destroy();

			VoxelInstancerRigidBody *moved_rb = block.bodies[last_instance_index];
			if (moved_rb != rb) {
				moved_rb->set_instance_index(instance_index);
				block.bodies.write[instance_index] = moved_rb;
			}
		}

		--instance_index;

		// DEBUG
		// Ref<CubeMesh> cm;
		// cm.instance();
		// cm->set_size(Vector3(0.5, 0.5, 0.5));
		// MeshInstance *mi = memnew(MeshInstance);
		// mi->set_mesh(cm);
		// mi->set_transform(get_global_transform() *
		// 				  (Transform(Basis(), (block_pos << layer->lod_index).to_vec3()) * t));
		// add_child(mi);
	}

	if (instance_count < initial_instance_count) {
		// According to the docs, set_instance_count() resets the array so we only hide them instead
		multimesh->set_visible_instance_count(instance_count);

		if (block.bodies.size() > 0) {
			block.bodies.resize(instance_count);
		}

		// Array args;
		// args.push_back(instance_count);
		// args.push_back(initial_instance_count);
		// args.push_back(block_pos.to_vec3());
		// args.push_back(layer->lod_index);
		// args.push_back(multimesh->get_instance_count());
		// print_line(
		// 		String("Hiding instances from {0} to {1}. P: {2}, lod: {3}, total: {4}").format(args));
	}
}

void VoxelInstancer::remove_floating_scene_instances(Block &block, const Transform &parent_transform,
		Box3i p_voxel_box, const VoxelTool &voxel_tool, int block_size_po2) {
	const int initial_instance_count = block.scene_instances.size();
	int instance_count = initial_instance_count;

	const Transform block_global_transform = Transform(parent_transform.basis,
			parent_transform.xform((block.grid_position << block_size_po2).to_vec3()));

	// Let's check all instances one by one
	// Note: the fact we have to query VisualServer in and out is pretty bad though.
	// - We probably have to sync with its thread in MT mode
	// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
	for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
		SceneInstance instance = block.scene_instances[instance_index];
		ERR_CONTINUE(instance.root == nullptr);
		const Transform scene_transform = instance.root->get_transform();
		const Vector3i voxel_pos(Vector3i::from_floored(scene_transform.origin + block_global_transform.origin));

		if (!p_voxel_box.contains(voxel_pos)) {
			continue;
		}

		// 1-voxel cheap check without interpolation
		const float sdf = voxel_tool.get_voxel_f(voxel_pos);
		if (sdf < -0.1f) {
			// Still enough ground
			continue;
		}

		// Remove the MultiMesh instance
		const int last_instance_index = --instance_count;

		// TODO In the case of scene instances, we could use an overlap check or a signal.
		// Detach so it won't try to update our instances, we already do it here
		ERR_CONTINUE(instance.component == nullptr);
		// Not using detach_as_removed(),
		// this function is not marking the block as modified. It may be done by the caller.
		instance.component->detach();
		instance.root->queue_delete();

		SceneInstance moved_instance = block.scene_instances[last_instance_index];
		if (moved_instance.root != instance.root) {
			if (moved_instance.component == nullptr) {
				ERR_PRINT("Instance component should not be null");
			} else {
				moved_instance.component->set_instance_index(instance_index);
			}
			block.scene_instances.write[instance_index] = moved_instance;
		}

		--instance_index;
	}

	if (instance_count < initial_instance_count) {
		if (block.scene_instances.size() > 0) {
			block.scene_instances.resize(instance_count);
		}
	}
}

void VoxelInstancer::on_area_edited(Box3i p_voxel_box) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int render_block_size = _parent->get_mesh_block_size();
	const int data_block_size = _parent->get_data_block_size();

	Ref<VoxelTool> voxel_tool = _parent->get_voxel_tool();
	ERR_FAIL_COND(voxel_tool.is_null());
	voxel_tool->set_channel(VoxelBuffer::CHANNEL_SDF);

	const Transform parent_transform = get_global_transform();
	const int base_block_size_po2 = _parent->get_mesh_block_size_pow2();

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Box3i render_blocks_box = p_voxel_box.downscaled(render_block_size << lod_index);
		const Box3i data_blocks_box = p_voxel_box.downscaled(data_block_size << lod_index);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const Layer *layer = get_layer(*it);
			const std::vector<Block *> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer->lod_index;

			render_blocks_box.for_each_cell([layer,
													&blocks,
													voxel_tool,
													p_voxel_box,
													parent_transform,
													block_size_po2,
													&lod, data_blocks_box](
													Vector3i block_pos) {
				const unsigned int *iptr = layer->blocks.getptr(block_pos);
				if (iptr == nullptr) {
					// No instancing block here
					return;
				}

				Block *block = blocks[*iptr];
				ERR_FAIL_COND(block == nullptr);

				if (block->scene_instances.size() > 0) {
					remove_floating_scene_instances(
							*block, parent_transform, p_voxel_box, **voxel_tool, block_size_po2);
				} else {
					remove_floating_multimesh_instances(
							*block, parent_transform, p_voxel_box, **voxel_tool, block_size_po2);
				}

				// All instances have to be frozen as edited.
				// Because even if none of them were removed or added, the ground on which they can spawn has changed,
				// and at the moment we don't want unexpected instances to generate when loading back this area.
				data_blocks_box.for_each_cell([&lod](Vector3i data_block_pos) {
					lod.modified_blocks.set(data_block_pos, true);
				});
			});
		}
	}
}

// This is called if a user destroys or unparents the body node while it's still attached to the ground
void VoxelInstancer::on_body_removed(Vector3i data_block_position, unsigned int render_block_index, int instance_index) {
	Block *block = _blocks[render_block_index];
	CRASH_COND(block == nullptr);
	ERR_FAIL_INDEX(instance_index, block->bodies.size());

	if (block->multimesh_instance.is_valid()) {
		// Remove the multimesh instance

		Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
		ERR_FAIL_COND(multimesh.is_null());

		int visible_count = get_visible_instance_count(**multimesh);
		ERR_FAIL_COND(instance_index >= visible_count);

		--visible_count;
		// TODO This is terrible in MT mode! Think about keeping a local copy...
		const Transform last_trans = multimesh->get_instance_transform(visible_count);
		multimesh->set_instance_transform(instance_index, last_trans);
		multimesh->set_visible_instance_count(visible_count);
	}

	// Unregister the body
	int body_count = block->bodies.size();
	int last_instance_index = --body_count;
	VoxelInstancerRigidBody *moved_body = block->bodies[last_instance_index];
	if (instance_index != last_instance_index) {
		moved_body->set_instance_index(instance_index);
		block->bodies.write[instance_index] = moved_body;
	}
	block->bodies.resize(body_count);

	// Mark data block as modified
	const Layer *layer = get_layer(block->layer_id);
	CRASH_COND(layer == nullptr);
	Lod &lod = _lods[layer->lod_index];
	lod.modified_blocks.set(data_block_position, true);
}

void VoxelInstancer::on_scene_instance_removed(Vector3i data_block_position, unsigned int render_block_index, int instance_index) {
	Block *block = _blocks[render_block_index];
	CRASH_COND(block == nullptr);
	ERR_FAIL_INDEX(instance_index, block->bodies.size());

	// Unregister the scene instance
	int instance_count = block->scene_instances.size();
	int last_instance_index = --instance_count;
	SceneInstance moved_instance = block->scene_instances[last_instance_index];
	if (instance_index != last_instance_index) {
		ERR_FAIL_COND(moved_instance.component == nullptr);
		moved_instance.component->set_instance_index(instance_index);
		block->scene_instances.write[instance_index] = moved_instance;
	}
	block->scene_instances.resize(instance_count);

	// Mark data block as modified
	const Layer *layer = get_layer(block->layer_id);
	CRASH_COND(layer == nullptr);
	Lod &lod = _lods[layer->lod_index];
	lod.modified_blocks.set(data_block_position, true);
}

void VoxelInstancer::on_scene_instance_modified(Vector3i data_block_position, unsigned int render_block_index) {
	Block *block = _blocks[render_block_index];
	CRASH_COND(block == nullptr);

	// Mark data block as modified
	const Layer *layer = get_layer(block->layer_id);
	CRASH_COND(layer == nullptr);
	Lod &lod = _lods[layer->lod_index];
	lod.modified_blocks.set(data_block_position, true);
}

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
}

Dictionary VoxelInstancer::debug_get_instance_counts() const {
	Dictionary d;

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		ERR_FAIL_COND_V(block == nullptr, Dictionary());
		if (!block->multimesh_instance.is_valid()) {
			continue;
		}

		Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
		ERR_FAIL_COND_V(multimesh.is_null(), Dictionary());

		const int count = get_visible_instance_count(**multimesh);

		Variant *vptr = d.getptr(block->layer_id);
		if (vptr == nullptr) {
			d[block->layer_id] = count;

		} else {
			ERR_FAIL_COND_V(vptr->get_type() != Variant::INT, Dictionary());
			*vptr = vptr->operator signed int() + count;
		}
	}
	return d;
}

String VoxelInstancer::get_configuration_warning() const {
	if (_parent == nullptr) {
		return TTR("This node must be child of a VoxelLodTerrain.");
	}
	if (_library.is_null()) {
		return TTR("No library is assigned. A VoxelInstanceLibrary is needed to spawn items.");
	}
	if (_library->get_item_count() == 0) {
		return TTR("The assigned library is empty. Add items to it so they can be spawned.");
	}
	return "";
}

void VoxelInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "library"), &VoxelInstancer::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelInstancer::get_library);

	ClassDB::bind_method(D_METHOD("set_up_mode", "mode"), &VoxelInstancer::set_up_mode);
	ClassDB::bind_method(D_METHOD("get_up_mode"), &VoxelInstancer::get_up_mode);

	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelInstancer::debug_get_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_instance_counts"), &VoxelInstancer::debug_get_instance_counts);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE, "VoxelInstanceLibrary"),
			"set_library", "get_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "up_mode", PROPERTY_HINT_ENUM, "PositiveY,Sphere"),
			"set_up_mode", "get_up_mode");

	BIND_CONSTANT(MAX_LOD);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);
}
