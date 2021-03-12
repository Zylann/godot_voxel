#include "voxel_instancer.h"
#include "../../edition/voxel_tool.h"
#include "../../util/funcs.h"
#include "../../util/macros.h"
#include "../../util/profiling.h"
#include "../voxel_lod_terrain.h"

#include <scene/3d/camera.h>
#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>
#include <scene/main/viewport.h>
#include <algorithm>

class VoxelInstancerRigidBody : public RigidBody {
	GDCLASS(VoxelInstancerRigidBody, RigidBody);

public:
	VoxelInstancerRigidBody() {
		set_mode(RigidBody::MODE_STATIC);
	}

	void set_block_index(int block_index) {
		_block_index = block_index;
	}

	void set_instance_index(int instance_index) {
		_instance_index = instance_index;
	}

	void attach(VoxelInstancer *parent) {
		_parent = parent;
	}

	void detach_and_destroy() {
		_parent = nullptr;
		queue_delete();
	}

	// Note, for this the body must switch to convex shapes
	// void detach_and_become_rigidbody() {
	// 	//...
	// }

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_UNPARENTED:
				// The user could queue_free() that node in game,
				// so we have to notify the instancer to remove the multimesh instance and pointer
				if (_parent != nullptr) {
					_parent->on_body_removed(_block_index, _instance_index);
					_parent = nullptr;
				}
				break;
		}
	}

private:
	VoxelInstancer *_parent = nullptr;
	int _block_index = -1;
	int _instance_index = -1;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
			const int base_block_size_po2 = _parent->get_block_size_pow2();
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
	const int block_size = _parent->get_block_size();

	// Hardcoded LOD thresholds for now.
	// Can't really use pixel density because view distances are controlled by the main surface LOD octree
	{
		const int block_region_extent = _parent->get_block_region_extent();

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

		const VoxelInstanceLibraryItem *item = _library->get_item_const(block->layer_id);
		const int mesh_lod_count = item->get_mesh_lod_count();
		if (mesh_lod_count <= 1) {
			// This block has no LOD
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

void VoxelInstancer::update_visibility() {
	if (!is_inside_tree()) {
		return;
	}
	const bool visible = is_visible_in_tree();
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		block->multimesh_instance.set_visible(visible);
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
		_library->for_each_item([this](int id, const VoxelInstanceLibraryItem &item) {
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

	Ref<VoxelInstanceLibraryItem> item = _library->get_item(layer_id);
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

	// Update existing blocks
	for (size_t block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block *block = _blocks[block_index];
		if (block->layer_id != layer_id) {
			continue;
		}
		const int lod_index = block->lod_index;
		const Lod &lod = _lods[lod_index];
		if (lod.modified_blocks.has(block->grid_position) ||
				lod.loaded_instances_data.find(block->grid_position) != lod.loaded_instances_data.end()) {
			// Was authored, no regen on this
			continue;
		}

		_transform_cache.clear();

		Array surface_arrays = _parent->get_block_surface(block->grid_position, lod_index);

		const int lod_block_size = _parent->get_block_size() << lod_index;
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
				static_cast<VoxelInstanceGenerator::UpMode>(_up_mode));

		update_block_from_transforms(block_index, to_slice_const(_transform_cache),
				block->grid_position, layer, *item, layer_id, world, block_transform);
	}
}

void VoxelInstancer::update_layer_meshes(int layer_id) {
	Ref<VoxelInstanceLibraryItem> item = _library->get_item(layer_id);
	ERR_FAIL_COND(item.is_null());

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block *block = *it;
		if (block->layer_id != layer_id || !block->multimesh_instance.is_valid()) {
			continue;
		}
		block->multimesh_instance.set_material_override(item->get_material_override());
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

void VoxelInstancer::on_library_item_changed(int item_id, VoxelInstanceLibraryItem::ChangeType change) {
	ERR_FAIL_COND(_library.is_null());

	// TODO It's unclear yet if some code paths do the right thing in case instances got edited

	switch (change) {
		case VoxelInstanceLibraryItem::CHANGE_ADDED: {
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());
			add_layer(item_id, item->get_lod_index());
			regenerate_layer(item_id, true);
			update_configuration_warning();
		} break;

		case VoxelInstanceLibraryItem::CHANGE_REMOVED:
			remove_layer(item_id);
			update_configuration_warning();
			break;

		case VoxelInstanceLibraryItem::CHANGE_GENERATOR:
			regenerate_layer(item_id, false);
			break;

		case VoxelInstanceLibraryItem::CHANGE_VISUAL:
			update_layer_meshes(item_id);
			break;

		case VoxelInstanceLibraryItem::CHANGE_LOD_INDEX: {
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(item_id);
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

	memdelete(block);

	if (block != moved_block) {
		Layer *layer = get_layer(moved_block->layer_id);
		unsigned int *iptr = layer->blocks.getptr(moved_block->grid_position);
		CRASH_COND(iptr == nullptr);
		*iptr = block_index;
	}
}

void VoxelInstancer::on_block_data_loaded(Vector3i grid_position, unsigned int lod_index,
		std::unique_ptr<VoxelInstanceBlockData> instances) {

	ERR_FAIL_COND(lod_index >= _lods.size());
	Lod &lod = _lods[lod_index];
	lod.loaded_instances_data.insert(std::make_pair(grid_position, std::move(instances)));
}

void VoxelInstancer::on_block_enter(Vector3i grid_position, unsigned int lod_index, Array surface_arrays) {
	if (lod_index >= _lods.size()) {
		return;
	}

	Lod &lod = _lods[lod_index];

	auto it = lod.loaded_instances_data.find(grid_position);
	const VoxelInstanceBlockData *instances_data = nullptr;

	if (it != lod.loaded_instances_data.end()) {
		instances_data = it->second.get();
		ERR_FAIL_COND(instances_data == nullptr);

		// We could do this:
		// lod.loaded_instances_data.erase(grid_position);
		// but we'd loose the information that this block was edited, so keeping it for now
	}

	create_blocks(instances_data, grid_position, lod_index, surface_arrays);
}

void VoxelInstancer::on_block_exit(Vector3i grid_position, unsigned int lod_index) {
	if (lod_index >= _lods.size()) {
		return;
	}

	VOXEL_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];

	// If we loaded data there but it was never used, we'll unload it either way
	lod.loaded_instances_data.erase(grid_position);

	if (lod.modified_blocks.has(grid_position)) {
		save_block(grid_position, lod_index);
		lod.modified_blocks.erase(grid_position);
	}

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_id = *it;

		Layer *layer = get_layer(layer_id);
		CRASH_COND(layer == nullptr);

		const unsigned int *block_index_ptr = layer->blocks.getptr(grid_position);
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

void VoxelInstancer::update_block_from_transforms(int block_index, ArraySlice<const Transform> transforms,
		Vector3i grid_position, Layer *layer, const VoxelInstanceLibraryItem *item, uint16_t layer_id,
		World *world, const Transform &block_transform) {

	VOXEL_PROFILE_SCOPE();

	CRASH_COND(layer == nullptr);
	CRASH_COND(item == nullptr);

	// Get or create block
	Block *block = nullptr;
	if (block_index != -1) {
		block = _blocks[block_index];
	} else {
		block_index = create_block(layer, layer_id, grid_position);
		block = _blocks[block_index];
	}

	// Update multimesh

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
		PoolRealArray bulk_array = DirectMultiMeshInstance::make_transform_3d_bulk_array(transforms);
		multimesh->set_instance_count(transforms.size());
		multimesh->set_as_bulk_array(bulk_array);

		if (item->get_mesh_lod_count() > 0) {
			multimesh->set_mesh(item->get_mesh(item->get_mesh_lod_count() - 1));
		}

		if (!block->multimesh_instance.is_valid()) {
			block->multimesh_instance.create();
		}
		block->multimesh_instance.set_multimesh(multimesh);
		block->multimesh_instance.set_world(world);
		block->multimesh_instance.set_transform(block_transform);
		block->multimesh_instance.set_material_override(item->get_material_override());
	}

	// Update bodies
	const Vector<VoxelInstanceLibraryItem::CollisionShapeInfo> &collision_shapes = item->get_collision_shapes();
	if (collision_shapes.size() > 0) {
		VOXEL_PROFILE_SCOPE();

		// Add new bodies
		for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
			const Transform body_transform = block_transform * transforms[instance_index];

			VoxelInstancerRigidBody *body;

			if (instance_index < static_cast<unsigned int>(block->bodies.size())) {
				body = block->bodies.write[instance_index];

			} else {
				body = memnew(VoxelInstancerRigidBody);
				body->attach(this);
				body->set_instance_index(instance_index);
				body->set_block_index(block_index);

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

static bool contains_layer_id(const VoxelInstanceBlockData &instances_data, int id) {
	for (auto it = instances_data.layers.begin(); it != instances_data.layers.end(); ++it) {
		const VoxelInstanceBlockData::LayerData &layer = *it;
		if (layer.id == id) {
			return true;
		}
	}
	return false;
}

void VoxelInstancer::create_blocks(const VoxelInstanceBlockData *instances_data_ptr, Vector3i grid_position,
		int lod_index, Array surface_arrays) {

	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	Lod &lod = _lods[lod_index];
	const Transform parent_transform = get_global_transform();
	Ref<World> world_ref = get_world();
	ERR_FAIL_COND(world_ref.is_null());
	World *world = *world_ref;

	const int lod_block_size = _parent->get_block_size() << lod_index;
	const Transform block_local_transform = Transform(Basis(), (grid_position * lod_block_size).to_vec3());
	const Transform block_transform = parent_transform * block_local_transform;

	// Load from data if any
	if (instances_data_ptr != nullptr) {
		VOXEL_PROFILE_SCOPE();
		const VoxelInstanceBlockData &instances_data = *instances_data_ptr;

		for (auto layer_it = instances_data.layers.begin(); layer_it != instances_data.layers.end(); ++layer_it) {
			const VoxelInstanceBlockData::LayerData &layer_data = *layer_it;

			const VoxelInstanceLibraryItem *item = _library->get_item(layer_data.id);
			if (item == nullptr) {
				ERR_PRINT(String("Could not find associated layer ID {0} from loaded instances data")
								  .format(varray(layer_data.id)));
				continue;
			}

			Layer *layer = get_layer(layer_data.id);
			CRASH_COND(layer == nullptr);

			if (!item->is_persistent()) {
				// That layer is no longer persistent so we'll have to ignore authored data...
				WARN_PRINT(String("Layer ID={0} received loaded data but is no longer persistent. "
								  "Loaded data will be ignored.")
								   .format(varray(layer_data.id)));
				continue;
			}

			const unsigned int *block_index_ptr = layer->blocks.getptr(grid_position);
			if (block_index_ptr != nullptr) {
				// The block was already made?
				continue;
			}

			// TODO Don't create blocks if there are no transforms?

			static_assert(sizeof(VoxelInstanceBlockData::InstanceData) == sizeof(Transform),
					"Assuming instance data only contains a transform for now");
			ArraySlice<const Transform> transforms(
					reinterpret_cast<const Transform *>(layer_data.instances.data()), layer_data.instances.size());
			update_block_from_transforms(
					-1, transforms, grid_position, layer, item, layer_data.id, world, block_transform);
		}
	}

	// Generate other blocks
	{
		VOXEL_PROFILE_SCOPE();

		if (surface_arrays.size() == 0) {
			return;
		}

		PoolVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];
		if (vertices.size() == 0) {
			return;
		}

		PoolVector3Array normals = surface_arrays[ArrayMesh::ARRAY_NORMAL];
		ERR_FAIL_COND(normals.size() == 0);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const int layer_id = *it;

			const VoxelInstanceLibraryItem *item = _library->get_item(layer_id);
			CRASH_COND(item == nullptr);
			if (item->get_generator().is_null()) {
				continue;
			}

			Layer *layer = get_layer(layer_id);
			CRASH_COND(layer == nullptr);

			const unsigned int *block_index_ptr = layer->blocks.getptr(grid_position);
			if (block_index_ptr != nullptr) {
				// The block was already made?
				continue;
			}

			if (item->is_persistent() && instances_data_ptr != nullptr &&
					contains_layer_id(*instances_data_ptr, layer_id)) {
				// Don't generate, it received modified data
				continue;
			}

			_transform_cache.clear();

			item->get_generator()->generate_transforms(
					_transform_cache,
					grid_position,
					lod_index,
					layer_id,
					surface_arrays,
					block_local_transform,
					static_cast<VoxelInstanceGenerator::UpMode>(_up_mode));

			update_block_from_transforms(-1, to_slice_const(_transform_cache),
					grid_position, layer, item, layer_id, world, block_transform);
		}
	}
}

// This API can be confusing so I made a wrapper
static inline int get_visible_instance_count(Ref<MultiMesh> mm) {
	int visible_count = mm->get_visible_instance_count();
	if (visible_count == -1) {
		visible_count = mm->get_instance_count();
	}
	return visible_count;
}

void VoxelInstancer::save_block(Vector3i grid_pos, int lod_index) const {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	PRINT_VERBOSE(String("Requesting save of instance block {0} lod {1}")
						  .format(varray(grid_pos.to_vec3(), lod_index)));

	const Lod &lod = _lods[lod_index];

	std::unique_ptr<VoxelInstanceBlockData> data = std::make_unique<VoxelInstanceBlockData>();
	const int block_size = _parent->get_block_size() << lod_index;
	data->position_range = block_size;

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_id = *it;

		const VoxelInstanceLibraryItem *item = _library->get_item_const(layer_id);
		CRASH_COND(item == nullptr);
		if (!item->is_persistent()) {
			continue;
		}

		const Layer *layer = get_layer_const(layer_id);
		CRASH_COND(layer == nullptr);

		ERR_FAIL_COND(layer_id < 0);

		const unsigned int *block_index_ptr = layer->blocks.getptr(grid_pos);

		if (block_index_ptr != nullptr) {
			const unsigned int block_index = *block_index_ptr;

#ifdef DEBUG_ENABLED
			CRASH_COND(block_index >= _blocks.size());
#endif
			Block *block = _blocks[block_index];

			data->layers.push_back(VoxelInstanceBlockData::LayerData());
			VoxelInstanceBlockData::LayerData &layer_data = data->layers.back();

			layer_data.id = layer_id;

			if (item->get_generator().is_valid()) {
				layer_data.scale_min = item->get_generator()->get_min_scale();
				layer_data.scale_max = item->get_generator()->get_max_scale();
			} else {
				// TODO Calculate scale range automatically in the serializer
				layer_data.scale_min = 0.1f;
				layer_data.scale_max = 10.f;
			}

			if (block->multimesh_instance.is_valid()) {
				Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
				CRASH_COND(multimesh.is_null());

				const int instance_count = get_visible_instance_count(multimesh);

				layer_data.instances.resize(instance_count);

				// TODO Optimization: it would be nice to get the whole array at once
				for (int i = 0; i < instance_count; ++i) {
					VOXEL_PROFILE_SCOPE();
					layer_data.instances[i].transform = multimesh->get_instance_transform(i);
				}
			}
		}
	}

	const int volume_id = _parent->get_volume_id();
	VoxelServer::get_singleton()->request_instance_block_save(volume_id, std::move(data), grid_pos, lod_index);
}

void VoxelInstancer::on_area_edited(Rect3i p_voxel_box) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int block_size = _parent->get_block_size();

	Ref<VoxelTool> voxel_tool = _parent->get_voxel_tool();
	voxel_tool->set_channel(VoxelBuffer::CHANNEL_SDF);

	const Transform parent_transform = get_global_transform();
	const int base_block_size_po2 = _parent->get_block_size_pow2();

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Rect3i blocks_box = p_voxel_box.downscaled(block_size << lod_index);

		for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
			const Layer *layer = get_layer(*it);
			const std::vector<Block *> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer->lod_index;

			blocks_box.for_each_cell([layer, &blocks, voxel_tool, p_voxel_box, parent_transform, block_size_po2, &lod](
											 Vector3i block_pos) {
				const unsigned int *iptr = layer->blocks.getptr(block_pos);
				if (iptr == nullptr) {
					// No instancing block here
					return;
				}

				Block *block = blocks[*iptr];
				if (!block->multimesh_instance.is_valid()) {
					// Empty block
					return;
				}

				Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
				ERR_FAIL_COND(multimesh.is_null());

				int initial_instance_count = get_visible_instance_count(multimesh);
				int instance_count = initial_instance_count;

				const Transform block_global_transform = Transform(parent_transform.basis,
						parent_transform.xform((block_pos << block_size_po2).to_vec3()));

				// Let's check all instances one by one
				// Note: the fact we have to query VisualServer in and out is pretty bad though.
				// - We probably have to sync with its thread in MT mode
				// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const Transform mm_transform = multimesh->get_instance_transform(instance_index);
					const Vector3i voxel_pos(mm_transform.origin + block_global_transform.origin);

					if (!p_voxel_box.contains(voxel_pos)) {
						continue;
					}

					// 1-voxel cheap check without interpolation
					const float sdf = voxel_tool->get_voxel_f(voxel_pos);
					if (sdf < -0.1f) {
						// Still enough ground
						continue;
					}

					// Remove the MultiMesh instance
					const int last_instance_index = --instance_count;
					const Transform last_trans = multimesh->get_instance_transform(last_instance_index);
					multimesh->set_instance_transform(instance_index, last_trans);

					// Remove the body if this block has some
					// TODO In the case of bodies, we could use an overlap check
					if (block->bodies.size() > 0) {
						VoxelInstancerRigidBody *rb = block->bodies[instance_index];
						// Detach so it won't try to update our instances, we already do it here
						rb->detach_and_destroy();

						VoxelInstancerRigidBody *moved_rb = block->bodies[last_instance_index];
						if (moved_rb != rb) {
							moved_rb->set_instance_index(instance_index);
							block->bodies.write[instance_index] = moved_rb;
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

				// All instances have to be frozen as edited.
				// Because even if none of them were removed or added, the ground on which they can spawn has changed,
				// and at the moment we don't want unexpected instances to generate when loading back this area.
				lod.modified_blocks.set(block_pos, true);

				if (instance_count < initial_instance_count) {
					// According to the docs, set_instance_count() resets the array so we only hide them instead
					multimesh->set_visible_instance_count(instance_count);

					if (block->bodies.size() > 0) {
						block->bodies.resize(instance_count);
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
			});
		}
	}
}

// This is called if a user destroys or unparents the body node while it's still attached to the ground
void VoxelInstancer::on_body_removed(unsigned int block_index, int instance_index) {
	ERR_FAIL_INDEX(block_index, _blocks.size());
	Block *block = _blocks[block_index];
	CRASH_COND(block == nullptr);
	ERR_FAIL_INDEX(instance_index, block->bodies.size());

	if (block->multimesh_instance.is_valid()) {
		// Remove the multimesh instance

		Ref<MultiMesh> multimesh = block->multimesh_instance.get_multimesh();
		ERR_FAIL_COND(multimesh.is_null());

		int visible_count = get_visible_instance_count(multimesh);
		ERR_FAIL_COND(instance_index >= visible_count);

		--visible_count;
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

	// Mark block as modified
	const Layer *layer = get_layer(block->layer_id);
	CRASH_COND(layer == nullptr);
	Lod &lod = _lods[layer->lod_index];
	lod.modified_blocks.set(block->grid_position, true);
}

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
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

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE, "VoxelInstanceLibrary"),
			"set_library", "get_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "up_mode", PROPERTY_HINT_ENUM, "PositiveY,Sphere"),
			"set_up_mode", "get_up_mode");

	BIND_CONSTANT(MAX_LOD);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);
}
