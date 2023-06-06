#include "../../edition/voxel_tool.h"
#include "../../engine/save_block_data_task.h"
#include "../../util/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/collision_shape_3d.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/multimesh.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/ref_counted.h"
#include "../../util/godot/classes/resource_saver.h"
#include "../../util/godot/classes/viewport.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "../fixed_lod/voxel_terrain.h"
#include "../variable_lod/voxel_lod_terrain.h"
#include "voxel_instance_component.h"
#include "voxel_instance_library_scene_item.h"
#include "voxel_instancer_rigidbody.h"

// Only needed for debug purposes, otherwise RenderingServer is used directly
#include "../../util/godot/classes/multimesh_instance_3d.h"

#include <algorithm>

namespace zylann::voxel {

namespace {
std::vector<Transform3f> &get_tls_transform_cache() {
	static thread_local std::vector<Transform3f> tls_transform_cache;
	return tls_transform_cache;
}
} // namespace

VoxelInstancer::VoxelInstancer() {
	set_notify_transform(true);
	set_process_internal(true);
	_generator_results = make_shared_instance<VoxelInstancerGeneratorTaskOutputQueue>();
}

VoxelInstancer::~VoxelInstancer() {
	// Destroy everything
	// Note: we don't destroy instances using nodes, we assume they were detached already

	if (_library.is_valid()) {
		_library->remove_listener(this);
	}
}

void VoxelInstancer::clear_blocks() {
	ZN_PROFILE_SCOPE();
	// Destroy blocks, keep configured layers
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;
		for (unsigned int i = 0; i < block.bodies.size(); ++i) {
			VoxelInstancerRigidBody *body = block.bodies[i];
			body->detach_and_destroy();
		}
		for (unsigned int i = 0; i < block.scene_instances.size(); ++i) {
			SceneInstance instance = block.scene_instances[i];
			ERR_CONTINUE(instance.component == nullptr);
			instance.component->detach();
			ERR_CONTINUE(instance.root == nullptr);
			instance.root->queue_free();
		}
	}
	_blocks.clear();
	for (auto it = _layers.begin(); it != _layers.end(); ++it) {
		Layer &layer = it->second;
		layer.blocks.clear();
	}
	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		lod.modified_blocks.clear();
	}
}

void VoxelInstancer::clear_blocks_in_layer(int layer_id) {
	// Not optimal, but should work for now
	for (size_t i = 0; i < _blocks.size(); ++i) {
		Block &block = *_blocks[i];
		if (block.layer_id == layer_id) {
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
			set_world(*get_world_3d());
			update_visibility();
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(get_world_3d().ptr());
#endif
			break;

		case NOTIFICATION_EXIT_WORLD:
			set_world(nullptr);
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(nullptr);
#endif
			break;

		case NOTIFICATION_PARENTED: {
			VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(get_parent());
			if (vlt != nullptr) {
				_parent = vlt;
				_parent_data_block_size_po2 = vlt->get_data_block_size_pow2();
				_parent_mesh_block_size_po2 = vlt->get_mesh_block_size_pow2();
				_mesh_lod_distance = vlt->get_lod_distance();
				vlt->set_instancer(this);
			} else {
				VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(get_parent());
				if (vt != nullptr) {
					_parent = vt;
					_parent_data_block_size_po2 = vt->get_data_block_size_pow2();
					_parent_mesh_block_size_po2 = vt->get_mesh_block_size_pow2();
					_mesh_lod_distance = vt->get_max_view_distance();
					vt->set_instancer(this);
				}
			}
			// TODO may want to reload all instances? Not sure if worth implementing that use case
		} break;

		case NOTIFICATION_UNPARENTED:
			clear_blocks();
			if (_parent != nullptr) {
				VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_parent);
				if (vlt != nullptr) {
					vlt->set_instancer(nullptr);
				} else {
					VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(get_parent());
					if (vt != nullptr) {
						vt->set_instancer(nullptr);
					}
				}
				_parent = nullptr;
			}
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			ZN_PROFILE_SCOPE_NAMED("VoxelInstancer::NOTIFICATION_TRANSFORM_CHANGED");

			if (!is_inside_tree() || _parent == nullptr) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			const Transform3D parent_transform = get_global_transform();
			const int base_block_size_po2 = _parent_mesh_block_size_po2;
			// print_line(String("IP: {0}").format(varray(parent_transform.origin)));

			for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
				Block &block = **it;
				if (!block.multimesh_instance.is_valid()) {
					// The block exists as an empty block (if it did not exist, it would get generated)
					continue;
				}
				const int block_size_po2 = base_block_size_po2 + block.lod_index;
				const Vector3 block_local_pos(block.grid_position << block_size_po2);
				// The local block transform never has rotation or scale so we can take a shortcut
				const Transform3D block_transform(parent_transform.basis, parent_transform.xform(block_local_pos));
				block.multimesh_instance.set_transform(block_transform);
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			update_visibility();
#ifdef TOOLS_ENABLED
			if (_gizmos_enabled) {
				_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
			}
#endif
			break;

		case NOTIFICATION_INTERNAL_PROCESS:
			process();
			break;
	}
}

void VoxelInstancer::process() {
	process_generator_results();
	if (_parent != nullptr && _library.is_valid() && _mesh_lod_distance > 0.f) {
		process_mesh_lods();
	}
#ifdef TOOLS_ENABLED
	if (_gizmos_enabled) {
		process_gizmos();
	}
#endif
}

void VoxelInstancer::process_generator_results() {
	ZN_PROFILE_SCOPE();
	static thread_local std::vector<VoxelInstanceGeneratorTaskOutput> tls_generator_results;
	std::vector<VoxelInstanceGeneratorTaskOutput> &results = tls_generator_results;
	if (results.size()) {
		ZN_PRINT_ERROR("Results were not cleaned up?");
	}
	{
		MutexLock mlock(_generator_results->mutex);
		// Copy results to temporary buffer
		std::vector<VoxelInstanceGeneratorTaskOutput> &src = _generator_results->results;
		results.resize(src.size());
		for (unsigned int i = 0; i < src.size(); ++i) {
			results[i] = std::move(src[i]);
		}
		src.clear();
	}

	if (results.size() == 0) {
		return;
	}

	Ref<World3D> maybe_world = get_world_3d();
	ERR_FAIL_COND(maybe_world.is_null());
	World3D &world = **maybe_world;

	const Transform3D parent_transform = get_global_transform();

	const int mesh_block_size_base = (1 << _parent_mesh_block_size_po2);

	for (VoxelInstanceGeneratorTaskOutput &output : results) {
		auto layer_it = _layers.find(output.layer_id);
		if (layer_it == _layers.end()) {
			// Layer was removed since?
			ZN_PRINT_VERBOSE("Processing async instance generator results, but the layer was removed.");
			continue;
		}
		Layer &layer = layer_it->second;

		const VoxelInstanceLibraryItem *item = _library->get_item(output.layer_id);
		CRASH_COND(item == nullptr);

		const int mesh_block_size = mesh_block_size_base << layer.lod_index;
		const Transform3D block_local_transform = Transform3D(Basis(), output.render_block_position * mesh_block_size);
		const Transform3D block_global_transform = parent_transform * block_local_transform;

		auto block_it = layer.blocks.find(output.render_block_position);
		if (block_it == layer.blocks.end()) {
			// The block was removed while the generation process was running?
			ZN_PRINT_VERBOSE("Processing async instance generator results, but the block was removed.");
			continue;
		}

		update_block_from_transforms(block_it->second, to_span_const(output.transforms), output.render_block_position,
				layer, *item, output.layer_id, world, block_global_transform, block_local_transform.origin);
	}

	results.clear();
}

#ifdef TOOLS_ENABLED

void VoxelInstancer::process_gizmos() {
	struct L {
		static inline void draw_box(DebugRenderer &dr, const Transform3D parent_transform, Vector3i bpos,
				unsigned int lod_index, unsigned int base_block_size_po2, Color8 color) {
			const int block_size_po2 = base_block_size_po2 + lod_index;
			const int block_size = 1 << block_size_po2;
			const Vector3 block_local_pos(bpos << block_size_po2);
			const Transform3D box_transform(
					parent_transform.basis * (Basis().scaled(Vector3(block_size, block_size, block_size))),
					parent_transform.xform(block_local_pos));
			dr.draw_box_mm(box_transform, color);
		}
	};

	ERR_FAIL_COND(_parent == nullptr);
	const Transform3D parent_transform = get_global_transform();
	const int base_mesh_block_size_po2 = _parent_mesh_block_size_po2;
	const int base_data_block_size_po2 = _parent_data_block_size_po2;

	_debug_renderer.begin();

	if (debug_get_draw_flag(DEBUG_DRAW_ALL_BLOCKS)) {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			const Block &block = **it;

			Color8 color(0, 255, 0, 255);
			if (block.multimesh_instance.is_valid()) {
				if (block.multimesh_instance.get_multimesh().is_null()) {
					// Allocated but without multimesh (wut?)
					color = Color8(128, 0, 0, 255);
				} else if (get_visible_instance_count(**block.multimesh_instance.get_multimesh()) == 0) {
					// Allocated but empty multimesh
					color = Color8(255, 64, 0, 255);
				}
			} else if (block.scene_instances.size() == 0) {
				// Only draw blocks that are setup
				continue;
			}

			L::draw_box(_debug_renderer, parent_transform, block.grid_position, block.lod_index,
					base_mesh_block_size_po2, color);
		}
	}

	if (debug_get_draw_flag(DEBUG_DRAW_EDITED_BLOCKS)) {
		for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
			const Lod &lod = _lods[lod_index];

			const Color8 edited_color(0, 255, 0, 255);
			const Color8 unsaved_color(255, 255, 0, 255);

			for (auto it = lod.loaded_instances_data.begin(); it != lod.loaded_instances_data.end(); ++it) {
				L::draw_box(_debug_renderer, parent_transform, it->first, lod_index, base_data_block_size_po2,
						edited_color);
			}

			for (auto it = lod.modified_blocks.begin(); it != lod.modified_blocks.end(); ++it) {
				L::draw_box(_debug_renderer, parent_transform, *it, lod_index, base_data_block_size_po2, unsaved_color);
			}
		}
	}

	_debug_renderer.end();
}

#endif

VoxelInstancer::Layer &VoxelInstancer::get_layer(int id) {
	auto it = _layers.find(id);
	ZN_ASSERT(it != _layers.end());
	return it->second;
}

const VoxelInstancer::Layer &VoxelInstancer::get_layer_const(int id) const {
	auto it = _layers.find(id);
	ZN_ASSERT(it != _layers.end());
	return it->second;
}

void VoxelInstancer::process_mesh_lods() {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	// Get viewer position
	const Viewport *viewport = get_viewport();
	const Camera3D *camera = viewport->get_camera_3d();
	if (camera == nullptr) {
		return;
	}
	const Transform3D gtrans = get_global_transform();
	const Vector3 cam_pos_local = (gtrans.affine_inverse() * camera->get_global_transform()).origin;

	ERR_FAIL_COND(_parent == nullptr);
	const int block_size = 1 << _parent_mesh_block_size_po2;

	{
		// Hardcoded LOD thresholds for now.
		// Can't really use pixel density because view distances are controlled by the main surface LOD octree
		FixedArray<float, 4> coeffs;
		coeffs[0] = 0;
		coeffs[1] = 0.1;
		coeffs[2] = 0.25;
		coeffs[3] = 0.5;
		const float hysteresis = 1.05;

		const float max_distance = _mesh_lod_distance;

		for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (unsigned int j = 0; j < lod.mesh_lod_distances.size(); ++j) {
				MeshLodDistances &mld = lod.mesh_lod_distances[j];
				const float lod_max_distance = (1 << j) * max_distance;
				mld.exit_distance_squared = lod_max_distance * lod_max_distance * coeffs[j];
				mld.enter_distance_squared = hysteresis * mld.exit_distance_squared;
			}
		}
	}

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;

		const VoxelInstanceLibraryItem *item_base = _library->get_item_const(block.layer_id);
		ERR_CONTINUE(item_base == nullptr);
		// TODO Optimization: would be nice to not need this cast by iterating only the same item types
		const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item_base);
		if (item == nullptr) {
			// Not a multimesh item
			continue;
		}
		const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();
		const int mesh_lod_count = settings.mesh_lod_count;
		if (mesh_lod_count <= 1) {
			// This block has no LOD
			// TODO Optimization: would be nice to not need this conditional by iterating only item types that define
			// lods
			continue;
		}

		const int lod_index = item->get_lod_index();

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(mesh_lod_count < VoxelInstanceLibraryMultiMeshItem::MAX_MESH_LODS);
#endif

		const Lod &lod = _lods[lod_index];

		const int lod_block_size = block_size << lod_index;
		const int hs = lod_block_size >> 1;
		const Vector3 block_center_local(block.grid_position * lod_block_size + Vector3i(hs, hs, hs));
		const float distance_squared = cam_pos_local.distance_squared_to(block_center_local);

		if (block.current_mesh_lod + 1 < mesh_lod_count &&
				distance_squared > lod.mesh_lod_distances[block.current_mesh_lod].enter_distance_squared) {
			// Decrease detail
			++block.current_mesh_lod;
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			if (multimesh.is_valid()) {
				multimesh->set_mesh(settings.mesh_lods[block.current_mesh_lod]);
			}
		}

		if (block.current_mesh_lod > 0 &&
				distance_squared < lod.mesh_lod_distances[block.current_mesh_lod].exit_distance_squared) {
			// Increase detail
			--block.current_mesh_lod;
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			if (multimesh.is_valid()) {
				multimesh->set_mesh(settings.mesh_lods[block.current_mesh_lod]);
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
		Block &block = **it;
		if (block.multimesh_instance.is_valid()) {
			block.multimesh_instance.set_visible(visible);
		}
	}
}

void VoxelInstancer::set_world(World3D *world) {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;
		if (block.multimesh_instance.is_valid()) {
			block.multimesh_instance.set_world(world);
		}
	}
}

void VoxelInstancer::set_up_mode(UpMode mode) {
	ERR_FAIL_COND(mode < 0 || mode >= UP_MODE_COUNT);
	if (_up_mode == mode) {
		return;
	}
	_up_mode = mode;
	for (auto it = _layers.begin(); it != _layers.end(); ++it) {
		regenerate_layer(it->first, false);
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

	update_configuration_warnings();
}

Ref<VoxelInstanceLibrary> VoxelInstancer::get_library() const {
	return _library;
}

void VoxelInstancer::regenerate_layer(uint16_t layer_id, bool regenerate_blocks) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);

	Ref<World3D> world_ref = get_world_3d();
	ERR_FAIL_COND(world_ref.is_null());
	World3D &world = **world_ref;

	Layer &layer = get_layer(layer_id);

	Ref<VoxelInstanceLibraryItem> item = _library->get_item(layer_id);
	ERR_FAIL_COND(item.is_null());
	if (item->get_generator().is_null()) {
		return;
	}

	const Transform3D parent_transform = get_global_transform();

	const VoxelLodTerrain *parent_vlt = Object::cast_to<VoxelLodTerrain>(_parent);
	const VoxelTerrain *parent_vt = Object::cast_to<VoxelTerrain>(_parent);

	if (regenerate_blocks) {
		// Create blocks
		std::vector<Vector3i> positions;

		if (parent_vlt != nullptr) {
			parent_vlt->get_meshed_block_positions_at_lod(layer.lod_index, positions);
		} else if (parent_vt != nullptr) {
			parent_vt->get_meshed_block_positions(positions);
		}

		for (unsigned int i = 0; i < positions.size(); ++i) {
			const Vector3i pos = positions[i];

			auto it = layer.blocks.find(pos);
			if (it != layer.blocks.end()) {
				continue;
			}

			create_block(layer, layer_id, pos, false);
		}
	}

	const int render_to_data_factor = 1 << (_parent_mesh_block_size_po2 - _parent_mesh_block_size_po2);
	ERR_FAIL_COND(render_to_data_factor <= 0 || render_to_data_factor > 2);

	struct L {
		// Does not return a bool so it can be used in bit-shifting operations without a compiler warning.
		// Can be treated like a bool too.
		static inline uint8_t has_edited_block(const Lod &lod, Vector3i pos) {
			return lod.modified_blocks.find(pos) != lod.modified_blocks.end() ||
					lod.loaded_instances_data.find(pos) != lod.loaded_instances_data.end();
		}

		static inline void extract_octant_transforms(
				const Block &render_block, std::vector<Transform3f> &dst, uint8_t octant_mask, int render_block_size) {
			if (!render_block.multimesh_instance.is_valid()) {
				return;
			}
			Ref<MultiMesh> multimesh = render_block.multimesh_instance.get_multimesh();
			ERR_FAIL_COND(multimesh.is_null());
			const int instance_count = get_visible_instance_count(**multimesh);
			const float h = render_block_size / 2;
			for (int i = 0; i < instance_count; ++i) {
				// TODO This is terrible in MT mode! Think about keeping a local copy...
				const Transform3D t = multimesh->get_instance_transform(i);
				const uint8_t octant_index = VoxelInstanceGenerator::get_octant_index(to_vec3f(t.origin), h);
				if ((octant_mask & (1 << octant_index)) != 0) {
					dst.push_back(to_transform3f(t));
				}
			}
		}
	};

	// Update existing blocks
	for (size_t block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block &block = *_blocks[block_index];
		if (block.layer_id != layer_id) {
			continue;
		}
		const int lod_index = block.lod_index;
		const Lod &lod = _lods[lod_index];

		// Each bit means "should this octant be generated". If 0, it means it was edited and should not change
		uint8_t octant_mask = 0xff;
		if (render_to_data_factor == 1) {
			if (L::has_edited_block(lod, block.grid_position)) {
				// Was edited, no regen on this
				continue;
			}
		} else if (render_to_data_factor == 2) {
			// The rendering block corresponds to 8 smaller data blocks
			uint8_t edited_mask = 0;
			const Vector3i data_pos0 = block.grid_position * render_to_data_factor;
			edited_mask |= L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y, data_pos0.z));
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y, data_pos0.z)) << 1);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y + 1, data_pos0.z)) << 2);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y + 1, data_pos0.z)) << 3);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y, data_pos0.z + 1)) << 4);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y, data_pos0.z + 1)) << 5);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x, data_pos0.y + 1, data_pos0.z + 1)) << 6);
			edited_mask |= (L::has_edited_block(lod, Vector3i(data_pos0.x + 1, data_pos0.y + 1, data_pos0.z + 1)) << 7);
			octant_mask = ~edited_mask;
			if (octant_mask == 0) {
				// All data blocks were edited, no regen on the whole render block
				continue;
			}
		}

		std::vector<Transform3f> &transform_cache = get_tls_transform_cache();
		transform_cache.clear();

		Array surface_arrays;
		if (parent_vlt != nullptr) {
			surface_arrays = parent_vlt->get_mesh_block_surface(block.grid_position, lod_index);
		} else if (parent_vt != nullptr) {
			surface_arrays = parent_vt->get_mesh_block_surface(block.grid_position);
		}

		const int mesh_block_size = 1 << _parent_mesh_block_size_po2;
		const int lod_block_size = mesh_block_size << lod_index;

		item->get_generator()->generate_transforms(transform_cache, block.grid_position, block.lod_index, layer_id,
				surface_arrays, static_cast<VoxelInstanceGenerator::UpMode>(_up_mode), octant_mask, lod_block_size);

		if (render_to_data_factor == 2 && octant_mask != 0xff) {
			// Complete transforms with edited ones
			L::extract_octant_transforms(block, transform_cache, ~octant_mask, mesh_block_size);
			// TODO What if these blocks had loaded data which wasn't yet uploaded for render?
			// We may setup a local transform list as well since it's expensive to get it from VisualServer
		}

		const Transform3D block_local_transform(Basis(), Vector3(block.grid_position * lod_block_size));
		const Transform3D block_transform = parent_transform * block_local_transform;

		update_block_from_transforms(block_index, to_span_const(transform_cache), block.grid_position, layer, **item,
				layer_id, world, block_transform, block_local_transform.origin);
	}
}

void VoxelInstancer::update_layer_meshes(int layer_id) {
	Ref<VoxelInstanceLibraryItem> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);

	const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;
		if (block.layer_id != layer_id || !block.multimesh_instance.is_valid()) {
			continue;
		}
		block.multimesh_instance.set_material_override(settings.material_override);
		block.multimesh_instance.set_cast_shadows_setting(settings.shadow_casting_setting);
		Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
		if (multimesh.is_valid()) {
			Ref<Mesh> mesh;
			if (settings.mesh_lod_count <= 1) {
				mesh = settings.mesh_lods[0];
			} else {
				mesh = settings.mesh_lods[block.current_mesh_lod];
			}
			multimesh->set_mesh(mesh);
		}
	}
}

void VoxelInstancer::update_layer_scenes(int layer_id) {
	Ref<VoxelInstanceLibraryItem> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	VoxelInstanceLibrarySceneItem *item = Object::cast_to<VoxelInstanceLibrarySceneItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);
	const int data_block_size_po2 = _parent_data_block_size_po2;

	for (unsigned int block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block &block = *_blocks[block_index];

		for (unsigned int instance_index = 0; instance_index < block.scene_instances.size(); ++instance_index) {
			SceneInstance prev_instance = block.scene_instances[instance_index];
			ERR_CONTINUE(prev_instance.root == nullptr);
			SceneInstance instance = create_scene_instance(
					*item, instance_index, block_index, prev_instance.root->get_transform(), data_block_size_po2);
			ERR_CONTINUE(instance.root == nullptr);
			block.scene_instances[instance_index] = instance;
			// We just drop the instance without saving, because this function is supposed to occur only in editor,
			// or in the very rare cases where library is modified in game (which would invalidate saves anyways).
			prev_instance.root->queue_free();
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
			update_configuration_warnings();
		} break;

		case VoxelInstanceLibraryItem::CHANGE_REMOVED:
			remove_layer(item_id);
			update_configuration_warnings();
			break;

		case VoxelInstanceLibraryItem::CHANGE_GENERATOR:
			regenerate_layer(item_id, false);
			break;

		case VoxelInstanceLibraryItem::CHANGE_VISUAL:
			update_layer_meshes(item_id);
			break;

		case VoxelInstanceLibraryItem::CHANGE_SCENE:
			update_layer_scenes(item_id);
			break;

		case VoxelInstanceLibraryItem::CHANGE_LOD_INDEX: {
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());

			clear_blocks_in_layer(item_id);

			Layer &layer = get_layer(item_id);

			Lod &prev_lod = _lods[layer.lod_index];
			unordered_remove_value(prev_lod.layers, item_id);

			layer.lod_index = item->get_lod_index();

			Lod &new_lod = _lods[layer.lod_index];
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
	ERR_FAIL_COND_MSG(_layers.find(layer_id) != _layers.end(), "Trying to add a layer that already exists");
#endif

	Lod &lod = _lods[lod_index];

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(std::find(lod.layers.begin(), lod.layers.end(), layer_id) != lod.layers.end(),
			"Layer already referenced by this LOD");
#endif

	Layer layer;
	layer.lod_index = lod_index;
	_layers.insert({ layer_id, layer });

	lod.layers.push_back(layer_id);
}

void VoxelInstancer::remove_layer(int layer_id) {
	Layer &layer = get_layer(layer_id);

	// Unregister that layer from the corresponding LOD structure
	Lod &lod = _lods[layer.lod_index];
	for (size_t i = 0; i < lod.layers.size(); ++i) {
		if (lod.layers[i] == layer_id) {
			lod.layers[i] = lod.layers.back();
			lod.layers.pop_back();
			break;
		}
	}

	clear_blocks_in_layer(layer_id);

	_layers.erase(layer_id);
}

void VoxelInstancer::remove_block(unsigned int block_index) {
#ifdef DEBUG_ENABLED
	CRASH_COND(block_index >= _blocks.size());
#endif
	// We will move the last block to the index previously occupied by the removed block.
	// It is cheaper than offsetting every block in the array.
	// Get this reference first, because if we are removing the last block, its address will become null due to move
	// semantics
	const Block &moved_block = *_blocks.back();

	UniquePtr<Block> block = std::move(_blocks[block_index]);
	{
		Layer &layer = get_layer(block->layer_id);
		layer.blocks.erase(block->grid_position);
	}
	_blocks[block_index] = std::move(_blocks.back());
	_blocks.pop_back();

	// Destroy objects linked to the block

	for (unsigned int i = 0; i < block->bodies.size(); ++i) {
		VoxelInstancerRigidBody *body = block->bodies[i];
		body->detach_and_destroy();
	}

	for (unsigned int i = 0; i < block->scene_instances.size(); ++i) {
		SceneInstance instance = block->scene_instances[i];
		ERR_CONTINUE(instance.component == nullptr);
		instance.component->detach();
		ERR_CONTINUE(instance.root == nullptr);
		instance.root->queue_free();
	}

	// If the block we removed was also the last one, we don't enter here
	if (block.get() != &moved_block) {
		// Update the index of the moved block referenced in its layer
		Layer &layer = get_layer(moved_block.layer_id);
		auto it = layer.blocks.find(moved_block.grid_position);
		CRASH_COND(it == layer.blocks.end());
		it->second = block_index;
	}
}

void VoxelInstancer::on_data_block_loaded(
		Vector3i grid_position, unsigned int lod_index, UniquePtr<InstanceBlockData> instances) {
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

	ZN_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];

	BufferedTaskScheduler &tasks = BufferedTaskScheduler::get_for_current_thread();

	const bool can_save = _parent == nullptr || _parent->get_stream().is_valid();

	// Remove data blocks
	const int render_to_data_factor = 1 << (_parent_mesh_block_size_po2 - _parent_data_block_size_po2);
	ERR_FAIL_COND(render_to_data_factor <= 0 || render_to_data_factor > 2);
	const Vector3i data_min_pos = render_grid_position * render_to_data_factor;
	const Vector3i data_max_pos = data_min_pos + Vector3iUtil::create(render_to_data_factor);
	Vector3i data_grid_pos;
	for (data_grid_pos.z = data_min_pos.z; data_grid_pos.z < data_max_pos.z; ++data_grid_pos.z) {
		for (data_grid_pos.y = data_min_pos.y; data_grid_pos.y < data_max_pos.y; ++data_grid_pos.y) {
			for (data_grid_pos.x = data_min_pos.x; data_grid_pos.x < data_max_pos.x; ++data_grid_pos.x) {
				// If we loaded data there but it was never used, we'll unload it either way.
				// Note, this data is what we loaded initially, it doesnt contain modifications.
				lod.loaded_instances_data.erase(data_grid_pos);

				auto modified_block_it = lod.modified_blocks.find(data_grid_pos);
				if (modified_block_it != lod.modified_blocks.end()) {
					if (can_save) {
						SaveBlockDataTask *task = save_block(data_grid_pos, lod_index, nullptr);
						if (task != nullptr) {
							tasks.push_io_task(task);
						}
					}
					lod.modified_blocks.erase(modified_block_it);
				}
			}
		}
	}

	tasks.flush();

	// Remove render blocks
	for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
		const int layer_id = *layer_it;

		Layer &layer = get_layer(layer_id);

		auto block_it = layer.blocks.find(render_grid_position);
		if (block_it != layer.blocks.end()) {
			remove_block(block_it->second);
		}
	}
}

void VoxelInstancer::save_all_modified_blocks(
		BufferedTaskScheduler &tasks, std::shared_ptr<AsyncDependencyTracker> tracker) {
	ZN_DSTACK();

	ZN_ASSERT_RETURN(_parent != nullptr);
	const bool can_save = _parent->get_stream().is_valid();
	ZN_ASSERT_RETURN_MSG(can_save,
			format("Cannot save instances, the parent {} has no {} assigned.", _parent->get_class(),
					String(VoxelStream::get_class_static())));

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		for (auto it = lod.modified_blocks.begin(); it != lod.modified_blocks.end(); ++it) {
			SaveBlockDataTask *task = save_block(*it, lod_index, tracker);
			if (task != nullptr) {
				tasks.push_io_task(task);
			}
		}
		lod.modified_blocks.clear();
	}
}

VoxelInstancer::SceneInstance VoxelInstancer::create_scene_instance(const VoxelInstanceLibrarySceneItem &scene_item,
		int instance_index, unsigned int block_index, Transform3D transform, int data_block_size_po2) {
	SceneInstance instance;
	ERR_FAIL_COND_V_MSG(scene_item.get_scene().is_null(), instance,
			String("{0} ({1}) is missing an attached scene in {2} ({3})")
					.format(varray(VoxelInstanceLibrarySceneItem::get_class_static(), scene_item.get_item_name(),
									VoxelInstancer::get_class_static()),
							get_path()));
	Node *root = scene_item.get_scene()->instantiate();
	ERR_FAIL_COND_V(root == nullptr, instance);
	instance.root = Object::cast_to<Node3D>(root);
	ERR_FAIL_COND_V_MSG(instance.root == nullptr, instance, "Root of scene instance must be derived from Spatial");

	instance.component = VoxelInstanceComponent::find_in(instance.root);
	if (instance.component == nullptr) {
		instance.component = memnew(VoxelInstanceComponent);
		instance.root->add_child(instance.component);
	}

	instance.component->attach(this);
	instance.component->set_instance_index(instance_index);
	instance.component->set_render_block_index(block_index);
	instance.component->set_data_block_position(math::floor_to_int(transform.origin) >> data_block_size_po2);

	instance.root->set_transform(transform);

	// This is the SLOWEST part because Godot triggers all sorts of callbacks
	add_child(instance.root);

	return instance;
}

unsigned int VoxelInstancer::create_block(
		Layer &layer, uint16_t layer_id, Vector3i grid_position, bool pending_instances) {
	UniquePtr<Block> block = make_unique_instance<Block>();
	block->layer_id = layer_id;
	block->current_mesh_lod = 0;
	block->lod_index = layer.lod_index;
	block->grid_position = grid_position;
	block->pending_instances = pending_instances;
	const unsigned int block_index = _blocks.size();
	_blocks.push_back(std::move(block));
#ifdef DEBUG_ENABLED
	// The block must not already exist
	CRASH_COND(layer.blocks.find(grid_position) != layer.blocks.end());
#endif
	layer.blocks.insert({ grid_position, block_index });
	return block_index;
}

void VoxelInstancer::update_block_from_transforms(int block_index, Span<const Transform3f> transforms,
		Vector3i grid_position, Layer &layer, const VoxelInstanceLibraryItem &item_base, uint16_t layer_id,
		World3D &world, const Transform3D &block_global_transform, Vector3 block_local_position) {
	ZN_PROFILE_SCOPE();

	// Get or create block
	if (block_index == -1) {
		block_index = create_block(layer, layer_id, grid_position, false);
	}
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(block_index < 0 || block_index >= int(_blocks.size()));
	ERR_FAIL_COND(_blocks[block_index] == nullptr);
#endif
	Block &block = *_blocks[block_index];

	// Update multimesh
	const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(&item_base);
	if (item != nullptr) {
		const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();

		if (transforms.size() == 0) {
			if (block.multimesh_instance.is_valid()) {
				block.multimesh_instance.set_multimesh(Ref<MultiMesh>());
				block.multimesh_instance.destroy();
			}

		} else {
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			if (multimesh.is_null()) {
				multimesh.instantiate();
				multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
				multimesh->set_use_colors(false);
				multimesh->set_use_custom_data(false);
			} else {
				multimesh->set_visible_instance_count(-1);
			}
			PackedFloat32Array bulk_array;
			DirectMultiMeshInstance::make_transform_3d_bulk_array(transforms, bulk_array);
			multimesh->set_instance_count(transforms.size());

			// Setting the mesh BEFORE `multimesh_set_buffer` because otherwise Godot computes the AABB inside
			// `multimesh_set_buffer` BY DOWNLOADING BACK THE BUFFER FROM THE GRAPHICS CARD which can incur a very harsh
			// performance penalty
			if (settings.mesh_lod_count > 0) {
				multimesh->set_mesh(settings.mesh_lods[settings.mesh_lod_count - 1]);
			}

			// TODO Waiting for Godot to expose the method on the resource object
			// multimesh->set_as_bulk_array(bulk_array);
			RenderingServer::get_singleton()->multimesh_set_buffer(multimesh->get_rid(), bulk_array);

			if (!block.multimesh_instance.is_valid()) {
				block.multimesh_instance.create();
				block.multimesh_instance.set_visible(is_visible());
			}
			block.multimesh_instance.set_multimesh(multimesh);
			block.multimesh_instance.set_world(&world);
			block.multimesh_instance.set_transform(block_global_transform);
			block.multimesh_instance.set_material_override(settings.material_override);
			block.multimesh_instance.set_cast_shadows_setting(settings.shadow_casting_setting);
		}

		// Update bodies
		Span<const VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo> collision_shapes =
				to_span(settings.collision_shapes);
		if (collision_shapes.size() > 0) {
			ZN_PROFILE_SCOPE_NAMED("Update multimesh bodies");

			const int data_block_size_po2 = _parent_data_block_size_po2;

			// Add new bodies
			for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
				const Transform3D local_transform = to_transform3(transforms[instance_index]);
				// Bodies are child nodes of the instancer, so we use local block coordinates
				const Transform3D body_transform(local_transform.basis, local_transform.origin + block_local_position);

				VoxelInstancerRigidBody *body;

				if (instance_index < static_cast<unsigned int>(block.bodies.size())) {
					body = block.bodies[instance_index];

				} else {
					// TODO Performance: removing nodes from the tree is slow. It causes framerate stalls.
					// See https://github.com/godotengine/godot/issues/61929
					// Instances with collisions can lead to the creation of thousands of nodes. While this works in
					// practice, removal proved to be very slow. Not because of physics, but because of an issue in the
					// node system itself. A possible workaround is to either use servers directly, or put nodes as
					// children of more nodes acting as buckets.
					body = memnew(VoxelInstancerRigidBody);
					body->attach(this);
					body->set_instance_index(instance_index);
					body->set_render_block_index(block_index);
					body->set_data_block_position(math::floor_to_int(body_transform.origin) >> data_block_size_po2);

					for (unsigned int i = 0; i < collision_shapes.size(); ++i) {
						const VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo &shape_info = collision_shapes[i];
						CollisionShape3D *cs = memnew(CollisionShape3D);
						cs->set_shape(shape_info.shape);
						cs->set_transform(shape_info.transform);
						body->add_child(cs);
					}

					add_child(body);
					block.bodies.push_back(body);
				}

				body->set_transform(body_transform);
			}

			// Remove old bodies
			for (unsigned int instance_index = transforms.size(); instance_index < block.bodies.size();
					++instance_index) {
				VoxelInstancerRigidBody *body = block.bodies[instance_index];
				body->detach_and_destroy();
			}

			block.bodies.resize(transforms.size());
		}
	}

	// Update scene instances
	const VoxelInstanceLibrarySceneItem *scene_item = Object::cast_to<VoxelInstanceLibrarySceneItem>(&item_base);
	if (scene_item != nullptr) {
		ZN_PROFILE_SCOPE_NAMED("Update scene instances");
		ERR_FAIL_COND_MSG(scene_item->get_scene().is_null(),
				String("{0} ({1}) is missing an attached scene in {2} ({3})")
						.format(varray(VoxelInstanceLibrarySceneItem::get_class_static(), item_base.get_item_name(),
										VoxelInstancer::get_class_static()),
								get_path()));

		const int data_block_size_po2 = _parent_data_block_size_po2;

		// Add new instances
		for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
			const Transform3D local_transform = to_transform3(transforms[instance_index]);
			const Transform3D body_transform(local_transform.basis, local_transform.origin + block_local_position);

			SceneInstance instance;

			if (instance_index < static_cast<unsigned int>(block.bodies.size())) {
				instance = block.scene_instances[instance_index];
				instance.root->set_transform(body_transform);

			} else {
				instance = create_scene_instance(
						*scene_item, instance_index, block_index, body_transform, data_block_size_po2);
				ERR_CONTINUE(instance.root == nullptr);
				block.scene_instances.push_back(instance);
			}

			// TODO Deserialize state
		}

		// Remove old instances
		for (unsigned int instance_index = transforms.size(); instance_index < block.scene_instances.size();
				++instance_index) {
			SceneInstance instance = block.scene_instances[instance_index];
			ERR_CONTINUE(instance.component == nullptr);
			instance.component->detach();
			ERR_CONTINUE(instance.root == nullptr);
			instance.root->queue_free();
		}

		block.scene_instances.resize(transforms.size());
	}
}

static const InstanceBlockData::LayerData *find_layer_data(const InstanceBlockData &instances_data, int id) {
	for (size_t i = 0; i < instances_data.layers.size(); ++i) {
		const InstanceBlockData::LayerData &layer = instances_data.layers[i];
		if (layer.id == id) {
			return &layer;
		}
	}
	return nullptr;
}

void VoxelInstancer::create_render_blocks(Vector3i render_grid_position, int lod_index, Array surface_arrays) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	// TODO Query one or multiple data blocks if any

	Lod &lod = _lods[lod_index];
	const Transform3D parent_transform = get_global_transform();
	Ref<World3D> maybe_world = get_world_3d();
	ERR_FAIL_COND(maybe_world.is_null());
	World3D &world = **maybe_world;

	const int mesh_block_size_base = (1 << _parent_mesh_block_size_po2);
	const int data_block_size_base = (1 << _parent_data_block_size_po2);
	const int mesh_block_size = mesh_block_size_base << lod_index;
	const int data_block_size = data_block_size_base << lod_index;

	const Transform3D block_local_transform = Transform3D(Basis(), render_grid_position * mesh_block_size);
	const Transform3D block_global_transform = parent_transform * block_local_transform;

	const int render_to_data_factor = mesh_block_size_base / data_block_size_base;
	const Vector3i data_min_pos = render_grid_position * render_to_data_factor;
	const Vector3i data_max_pos = data_min_pos + Vector3iUtil::create(render_to_data_factor);

	std::vector<Transform3f> &transform_cache = get_tls_transform_cache();

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
		const int layer_id = *layer_it;

		Layer &layer = get_layer(layer_id);

		if (layer.blocks.find(render_grid_position) != layer.blocks.end()) {
			// The block was already made?
			continue;
		}

		transform_cache.clear();

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

						const InstanceBlockData *instances_data = instances_data_it->second.get();
						CRASH_COND(instances_data == nullptr);

						const InstanceBlockData::LayerData *layer_data = find_layer_data(*instances_data, layer_id);

						if (layer_data == nullptr) {
							continue;
						}

						for (auto it = layer_data->instances.begin(); it != layer_data->instances.end(); ++it) {
							transform_cache.push_back(it->transform);
						}

						if (render_to_data_factor != 1) {
							// Data blocks store instances relative to a smaller grid than render blocks.
							// So we need to adjust their relative position.
							const Vector3f rel = to_vec3f((data_grid_pos - data_min_pos) * data_block_size);
							const size_t cache_begin_index = transform_cache.size() - layer_data->instances.size();
							ZN_ASSERT(cache_begin_index <= transform_cache.size());
							for (auto it = transform_cache.begin() + cache_begin_index; it != transform_cache.end();
									++it) {
								it->origin += rel;
							}
						}

						// Unset bit for this octant so it won't be generated
						gen_octant_mask &= ~(1 << octant_index);
					}

					++octant_index;
				}
			}
		}

		const VoxelInstanceLibraryItem *item = _library->get_item(layer_id);
		CRASH_COND(item == nullptr);

		// Generate the rest
		bool pending_generation = false;
		if (gen_octant_mask != 0 && surface_arrays.size() != 0 && item->get_generator().is_valid()) {
			PackedVector3Array vertices = surface_arrays[ArrayMesh::ARRAY_VERTEX];

			if (vertices.size() != 0) {
				GenerateInstancesBlockTask *task = ZN_NEW(GenerateInstancesBlockTask);
				task->mesh_block_grid_position = render_grid_position;
				task->layer_id = layer_id;
				task->mesh_block_size = mesh_block_size;
				task->lod_index = lod_index;
				task->gen_octant_mask = gen_octant_mask;
				task->up_mode = _up_mode;
				task->surface_arrays = surface_arrays;
				task->generator = item->get_generator();
				task->transforms = transform_cache;
				task->output_queue = _generator_results;

				task_scheduler.push_main_task(task);

				pending_generation = true;
			}
		}

		if (pending_generation) {
			// Create empty block in pending state
			create_block(layer, layer_id, render_grid_position, true);
		} else {
			// Create and populate block immediately
			update_block_from_transforms(-1, to_span_const(transform_cache), render_grid_position, layer, *item,
					layer_id, world, block_global_transform, block_local_transform.origin);
		}
	}

	task_scheduler.flush();
}

SaveBlockDataTask *VoxelInstancer::save_block(
		Vector3i data_grid_pos, int lod_index, std::shared_ptr<AsyncDependencyTracker> tracker) const {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(_library.is_null(), nullptr);
	ERR_FAIL_COND_V(_parent == nullptr, nullptr);

	ZN_PRINT_VERBOSE(format("Requesting save of instance block {} lod {}", data_grid_pos, lod_index));

	const Lod &lod = _lods[lod_index];

	UniquePtr<InstanceBlockData> block_data = make_unique_instance<InstanceBlockData>();
	const int data_block_size = (1 << _parent_data_block_size_po2) << lod_index;
	block_data->position_range = data_block_size;

	const int render_to_data_factor = (1 << _parent_mesh_block_size_po2) / (1 << _parent_data_block_size_po2);
	ERR_FAIL_COND_V_MSG(render_to_data_factor < 1 || render_to_data_factor > 2, nullptr, "Unsupported block size");

	const int render_block_size_base = (1 << _parent_mesh_block_size_po2);
	const int render_block_size = render_block_size_base << lod_index;
	const int half_render_block_size = render_block_size / 2;
	const Vector3i render_block_pos = math::floordiv(data_grid_pos, render_to_data_factor);

	const int octant_index =
			VoxelInstanceGenerator::get_octant_index(data_grid_pos.x & 1, data_grid_pos.y & 1, data_grid_pos.z & 1);

	for (auto it = lod.layers.begin(); it != lod.layers.end(); ++it) {
		const int layer_id = *it;

		const VoxelInstanceLibraryItem *item = _library->get_item_const(layer_id);
		CRASH_COND(item == nullptr);
		if (!item->is_persistent()) {
			continue;
		}

		const Layer &layer = get_layer_const(layer_id);

		ERR_FAIL_COND_V(layer_id < 0, nullptr);

		const auto render_block_it = layer.blocks.find(render_block_pos);
		if (render_block_it == layer.blocks.end()) {
			continue;
		}

		const unsigned int render_block_index = render_block_it->second;
#ifdef DEBUG_ENABLED
		CRASH_COND(render_block_index >= _blocks.size());
#endif
		Block &render_block = *_blocks[render_block_index];

		block_data->layers.push_back(InstanceBlockData::LayerData());
		InstanceBlockData::LayerData &layer_data = block_data->layers.back();

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

		if (render_block.multimesh_instance.is_valid()) {
			// Multimeshes

			Ref<MultiMesh> multimesh = render_block.multimesh_instance.get_multimesh();
			CRASH_COND(multimesh.is_null());

			ZN_PROFILE_SCOPE();

			const int instance_count = get_visible_instance_count(**multimesh);

			if (render_to_data_factor == 1) {
				layer_data.instances.resize(instance_count);

				// TODO Optimization: it would be nice to get the whole array at once
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO This is terrible in MT mode! Think about keeping a local copy...
					layer_data.instances[instance_index].transform =
							to_transform3f(multimesh->get_instance_transform(instance_index));
				}

			} else if (render_to_data_factor == 2) {
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO Optimize: This is terrible in MT mode! Think about keeping a local copy...
					const Transform3D rendered_instance_transform = multimesh->get_instance_transform(instance_index);
					const int instance_octant_index = VoxelInstanceGenerator::get_octant_index(
							to_vec3f(rendered_instance_transform.origin), half_render_block_size);
					if (instance_octant_index == octant_index) {
						InstanceBlockData::InstanceData d;
						d.transform = to_transform3f(rendered_instance_transform);
						layer_data.instances.push_back(d);
					}
				}
			}

		} else if (render_block.scene_instances.size() > 0) {
			// Scenes

			ZN_PROFILE_SCOPE();
			const unsigned int instance_count = render_block.scene_instances.size();

			const Vector3 render_block_origin = render_block_pos * render_block_size;

			if (render_to_data_factor == 1) {
				layer_data.instances.resize(instance_count);

				for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const SceneInstance instance = render_block.scene_instances[instance_index];
					ERR_CONTINUE(instance.root == nullptr);
					layer_data.instances[instance_index].transform =
							to_transform3f(instance.root->get_transform().translated(-render_block_origin));
				}

			} else if (render_to_data_factor == 2) {
				for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
					const SceneInstance instance = render_block.scene_instances[instance_index];
					ERR_CONTINUE(instance.root == nullptr);
					Transform3D t = instance.root->get_transform();
					t.origin -= render_block_origin;
					const int instance_octant_index =
							VoxelInstanceGenerator::get_octant_index(to_vec3f(t.origin), half_render_block_size);
					if (instance_octant_index == octant_index) {
						InstanceBlockData::InstanceData d;
						d.transform = to_transform3f(t);
						layer_data.instances.push_back(d);
					}
					// TODO Serialize state?
				}
			}

			// Make scene transforms relative to render block
			// for (InstanceBlockData::InstanceData &d : layer_data.instances) {
			// 	d.transform.origin -= render_block_origin;
			// }
		}

		if (render_to_data_factor == 2) {
			// Data blocks are on a smaller grid than render blocks so we may convert the relative position
			// of the instances
			const Vector3f rel = to_vec3f(data_block_size * (data_grid_pos - render_block_pos * render_to_data_factor));
			for (InstanceBlockData::InstanceData &d : layer_data.instances) {
				d.transform.origin -= rel;
			}
		}
	}

	const VolumeID volume_id = _parent->get_volume_id();

	std::shared_ptr<StreamingDependency> stream_dependency = _parent->get_streaming_dependency();
	ZN_ASSERT(stream_dependency != nullptr);

	SaveBlockDataTask *task = ZN_NEW(SaveBlockDataTask(
			volume_id, data_grid_pos, lod_index, data_block_size, std::move(block_data), stream_dependency, tracker));

	return task;
}

void VoxelInstancer::remove_floating_multimesh_instances(Block &block, const Transform3D &parent_transform,
		Box3i p_voxel_box, const VoxelTool &voxel_tool, int block_size_po2) {
	if (!block.multimesh_instance.is_valid()) {
		// Empty block
		return;
	}

	Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
	ERR_FAIL_COND(multimesh.is_null());

	const int initial_instance_count = get_visible_instance_count(**multimesh);
	int instance_count = initial_instance_count;

	const Transform3D block_global_transform =
			Transform3D(parent_transform.basis, parent_transform.xform(block.grid_position << block_size_po2));

	// Let's check all instances one by one
	// Note: the fact we have to query VisualServer in and out is pretty bad though.
	// - We probably have to sync with its thread in MT mode
	// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
	for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
		// TODO Optimize: This is terrible in MT mode! Think about keeping a local copy...
		const Transform3D mm_transform = multimesh->get_instance_transform(instance_index);
		const Vector3i voxel_pos(math::floor_to_int(mm_transform.origin + block_global_transform.origin));

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
		const Transform3D last_trans = multimesh->get_instance_transform(last_instance_index);
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
				block.bodies[instance_index] = moved_rb;
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

void VoxelInstancer::remove_floating_scene_instances(Block &block, const Transform3D &parent_transform,
		Box3i p_voxel_box, const VoxelTool &voxel_tool, int block_size_po2) {
	const unsigned int initial_instance_count = block.scene_instances.size();
	unsigned int instance_count = initial_instance_count;

	const Transform3D block_global_transform =
			Transform3D(parent_transform.basis, parent_transform.xform(block.grid_position << block_size_po2));

	// Let's check all instances one by one
	// Note: the fact we have to query VisualServer in and out is pretty bad though.
	// - We probably have to sync with its thread in MT mode
	// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
	for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
		SceneInstance instance = block.scene_instances[instance_index];
		ERR_CONTINUE(instance.root == nullptr);
		const Transform3D scene_transform = instance.root->get_transform();
		const Vector3i voxel_pos(math::floor_to_int(scene_transform.origin + block_global_transform.origin));

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
		const unsigned int last_instance_index = --instance_count;

		// TODO In the case of scene instances, we could use an overlap check or a signal.
		// Detach so it won't try to update our instances, we already do it here
		ERR_CONTINUE(instance.component == nullptr);
		// Not using detach_as_removed(),
		// this function is not marking the block as modified. It may be done by the caller.
		instance.component->detach();
		instance.root->queue_free();

		SceneInstance moved_instance = block.scene_instances[last_instance_index];
		if (moved_instance.root != instance.root) {
			if (moved_instance.component == nullptr) {
				ERR_PRINT("Instance component should not be null");
			} else {
				moved_instance.component->set_instance_index(instance_index);
			}
			block.scene_instances[instance_index] = moved_instance;
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
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int render_block_size = 1 << _parent_mesh_block_size_po2;
	const int data_block_size = 1 << _parent_data_block_size_po2;

	Ref<VoxelTool> maybe_voxel_tool = _parent->get_voxel_tool();
	ERR_FAIL_COND(maybe_voxel_tool.is_null());
	VoxelTool &voxel_tool = **maybe_voxel_tool;
	voxel_tool.set_channel(VoxelBufferInternal::CHANNEL_SDF);

	const Transform3D parent_transform = get_global_transform();
	const int base_block_size_po2 = 1 << _parent_mesh_block_size_po2;

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Box3i render_blocks_box = p_voxel_box.downscaled(render_block_size << lod_index);
		const Box3i data_blocks_box = p_voxel_box.downscaled(data_block_size << lod_index);

		for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
			const Layer &layer = get_layer(*layer_it);
			const std::vector<UniquePtr<Block>> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer.lod_index;

			render_blocks_box.for_each_cell([layer, &blocks, &voxel_tool, p_voxel_box, parent_transform, block_size_po2,
													&lod, data_blocks_box](Vector3i block_pos) {
				const auto block_it = layer.blocks.find(block_pos);
				if (block_it == layer.blocks.end()) {
					// No instancing block here
					return;
				}

				Block &block = *blocks[block_it->second];

				if (block.scene_instances.size() > 0) {
					remove_floating_scene_instances(block, parent_transform, p_voxel_box, voxel_tool, block_size_po2);
				} else {
					remove_floating_multimesh_instances(
							block, parent_transform, p_voxel_box, voxel_tool, block_size_po2);
				}

				// All instances have to be frozen as edited.
				// Because even if none of them were removed or added, the ground on which they can spawn has changed,
				// and at the moment we don't want unexpected instances to generate when loading back this area.
				data_blocks_box.for_each_cell([&lod](Vector3i data_block_pos) { //
					lod.modified_blocks.insert(data_block_pos);
				});
			});
		}
	}
}

// This is called if a user destroys or unparents the body node while it's still attached to the ground
void VoxelInstancer::on_body_removed(
		Vector3i data_block_position, unsigned int render_block_index, unsigned int instance_index) {
	Block &block = *_blocks[render_block_index];
	ERR_FAIL_INDEX(instance_index, block.bodies.size());

	if (block.multimesh_instance.is_valid()) {
		// Remove the multimesh instance

		Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
		ERR_FAIL_COND(multimesh.is_null());

		int visible_count = get_visible_instance_count(**multimesh);
		ERR_FAIL_COND(int(instance_index) >= visible_count);

		--visible_count;
		// TODO Optimize: This is terrible in MT mode! Think about keeping a local copy...
		const Transform3D last_trans = multimesh->get_instance_transform(visible_count);
		multimesh->set_instance_transform(instance_index, last_trans);
		multimesh->set_visible_instance_count(visible_count);
	}

	// Unregister the body
	unsigned int body_count = block.bodies.size();
	const unsigned int last_instance_index = --body_count;
	VoxelInstancerRigidBody *moved_body = block.bodies[last_instance_index];
	if (instance_index != last_instance_index) {
		moved_body->set_instance_index(instance_index);
		block.bodies[instance_index] = moved_body;
	}
	block.bodies.resize(body_count);

	// Mark data block as modified
	const Layer &layer = get_layer(block.layer_id);
	Lod &lod = _lods[layer.lod_index];
	lod.modified_blocks.insert(data_block_position);
}

void VoxelInstancer::on_scene_instance_removed(
		Vector3i data_block_position, unsigned int render_block_index, unsigned int instance_index) {
	Block &block = *_blocks[render_block_index];
	ERR_FAIL_INDEX(instance_index, block.scene_instances.size());

	// Unregister the scene instance
	unsigned int instance_count = block.scene_instances.size();
	const unsigned int last_instance_index = --instance_count;
	SceneInstance moved_instance = block.scene_instances[last_instance_index];
	if (instance_index != last_instance_index) {
		ERR_FAIL_COND(moved_instance.component == nullptr);
		moved_instance.component->set_instance_index(instance_index);
		block.scene_instances[instance_index] = moved_instance;
	}
	block.scene_instances.resize(instance_count);

	// Mark data block as modified
	const Layer &layer = get_layer(block.layer_id);
	Lod &lod = _lods[layer.lod_index];
	lod.modified_blocks.insert(data_block_position);
}

void VoxelInstancer::on_scene_instance_modified(Vector3i data_block_position, unsigned int render_block_index) {
	Block &block = *_blocks[render_block_index];

	// Mark data block as modified
	const Layer &layer = get_layer(block.layer_id);
	Lod &lod = _lods[layer.lod_index];
	lod.modified_blocks.insert(data_block_position);
}

void VoxelInstancer::set_mesh_block_size_po2(unsigned int p_mesh_block_size_po2) {
	_parent_mesh_block_size_po2 = p_mesh_block_size_po2;
}

void VoxelInstancer::set_data_block_size_po2(unsigned int p_data_block_size_po2) {
	_parent_data_block_size_po2 = p_data_block_size_po2;
}

void VoxelInstancer::set_mesh_lod_distance(float p_lod_distance) {
	_mesh_lod_distance = p_lod_distance;
}

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
}

void VoxelInstancer::debug_get_instance_counts(std::unordered_map<uint32_t, uint32_t> &counts_per_layer) const {
	ZN_PROFILE_SCOPE();

	counts_per_layer.clear();

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		const Block &block = **it;

		uint32_t count = block.scene_instances.size();

		if (block.multimesh_instance.is_valid()) {
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			ZN_ASSERT_CONTINUE(multimesh.is_valid());

			count += get_visible_instance_count(**multimesh);
		}

		counts_per_layer[block.layer_id] += count;
	}
}

Dictionary VoxelInstancer::_b_debug_get_instance_counts() const {
	Dictionary d;
	std::unordered_map<uint32_t, uint32_t> map;
	debug_get_instance_counts(map);
	for (auto it = map.begin(); it != map.end(); ++it) {
		d[it->first] = it->second;
	}
	return d;
}

void VoxelInstancer::debug_dump_as_scene(String fpath) const {
	Node *root = debug_dump_as_nodes();
	ERR_FAIL_COND(root == nullptr);

	set_nodes_owner_except_root(root, root);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	const Error pack_result = packed_scene->pack(root);
	memdelete(root);
	ERR_FAIL_COND(pack_result != OK);

	const Error save_result = save_resource(packed_scene, fpath, ResourceSaver::FLAG_BUNDLE_RESOURCES);
	ERR_FAIL_COND(save_result != OK);
}

Node *VoxelInstancer::debug_dump_as_nodes() const {
	const unsigned int mesh_block_size = 1 << _parent_mesh_block_size_po2;

	Node3D *root = memnew(Node3D);
	root->set_transform(get_transform());
	root->set_name("VoxelInstancerRoot");

	std::unordered_map<Ref<Mesh>, Ref<Mesh>> mesh_copies;

	// For each layer
	for (auto layer_it = _layers.begin(); layer_it != _layers.end(); ++layer_it) {
		const Layer &layer = layer_it->second;
		const int lod_block_size = mesh_block_size << layer.lod_index;

		Node3D *layer_node = memnew(Node3D);
		layer_node->set_name(String("Layer{0}").format(varray(layer_it->first)));
		root->add_child(layer_node);

		// For each block in layer
		for (auto block_it = layer.blocks.begin(); block_it != layer.blocks.end(); ++block_it) {
			const unsigned int block_index = block_it->second;
			CRASH_COND(block_index >= _blocks.size());
			const Block &block = *_blocks[block_index];

			if (block.multimesh_instance.is_valid()) {
				const Transform3D block_local_transform(Basis(), Vector3(block.grid_position * lod_block_size));

				Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
				ERR_CONTINUE(multimesh.is_null());
				Ref<Mesh> src_mesh = multimesh->get_mesh();
				ERR_CONTINUE(src_mesh.is_null());

				// Duplicating the meshes because often they don't get saved even with `FLAG_BUNDLE_RESOURCES`
				auto mesh_copy_it = mesh_copies.find(src_mesh);
				Ref<Mesh> mesh_copy;
				if (mesh_copy_it == mesh_copies.end()) {
					mesh_copy = src_mesh->duplicate();
					mesh_copies.insert({ src_mesh, mesh_copy });
				} else {
					mesh_copy = mesh_copy_it->second;
				}

				Ref<MultiMesh> multimesh_copy = multimesh->duplicate();
				multimesh_copy->set_mesh(mesh_copy);

				MultiMeshInstance3D *mmi = memnew(MultiMeshInstance3D);
				mmi->set_multimesh(multimesh_copy);
				mmi->set_transform(block_local_transform);
				layer_node->add_child(mmi);
			}

			// TODO Dump scene instances too
		}
	}

	return root;
}

void VoxelInstancer::debug_set_draw_enabled(bool enabled) {
#ifdef TOOLS_ENABLED
	_gizmos_enabled = enabled;
	if (_gizmos_enabled) {
		if (is_inside_tree()) {
			_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
		}
	} else {
		_debug_renderer.clear();
	}
#endif
}

bool VoxelInstancer::debug_is_draw_enabled() const {
#ifdef TOOLS_ENABLED
	return _gizmos_enabled;
#else
	return false;
#endif
}

void VoxelInstancer::debug_set_draw_flag(DebugDrawFlag flag_index, bool enabled) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_INDEX(flag_index, DEBUG_DRAW_FLAGS_COUNT);
	if (enabled) {
		_debug_draw_flags |= (1 << flag_index);
	} else {
		_debug_draw_flags &= ~(1 << flag_index);
	}
#endif
}

bool VoxelInstancer::debug_get_draw_flag(DebugDrawFlag flag_index) const {
#ifdef TOOLS_ENABLED
	ERR_FAIL_INDEX_V(flag_index, DEBUG_DRAW_FLAGS_COUNT, false);
	return (_debug_draw_flags & (1 << flag_index)) != 0;
#else
	return false;
#endif
}

#ifdef TOOLS_ENABLED

#if defined(ZN_GODOT)
PackedStringArray VoxelInstancer::get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#elif defined(ZN_GODOT_EXTENSION)
PackedStringArray VoxelInstancer::_get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#endif

void VoxelInstancer::get_configuration_warnings(PackedStringArray &warnings) const {
	if (_parent == nullptr) {
		warnings.append(
				ZN_TTR("This node must be child of a {0}.").format(varray(VoxelLodTerrain::get_class_static())));
	}
	if (_library.is_null()) {
		warnings.append(ZN_TTR("No library is assigned. A {0} is needed to spawn items.")
								.format(varray(VoxelInstanceLibrary::get_class_static())));
	} else if (_library->get_item_count() == 0) {
		warnings.append(ZN_TTR("The assigned library is empty. Add items to it so they can be spawned."));
	}
}

#endif

void VoxelInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "library"), &VoxelInstancer::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelInstancer::get_library);

	ClassDB::bind_method(D_METHOD("set_up_mode", "mode"), &VoxelInstancer::set_up_mode);
	ClassDB::bind_method(D_METHOD("get_up_mode"), &VoxelInstancer::get_up_mode);

	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelInstancer::debug_get_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_instance_counts"), &VoxelInstancer::_b_debug_get_instance_counts);
	ClassDB::bind_method(D_METHOD("debug_dump_as_scene", "fpath"), &VoxelInstancer::debug_dump_as_scene);
	ClassDB::bind_method(D_METHOD("debug_set_draw_enabled", "enabled"), &VoxelInstancer::debug_set_draw_enabled);
	ClassDB::bind_method(D_METHOD("debug_is_draw_enabled"), &VoxelInstancer::debug_is_draw_enabled);
	ClassDB::bind_method(D_METHOD("debug_set_draw_flag", "flag", "enabled"), &VoxelInstancer::debug_set_draw_flag);
	ClassDB::bind_method(D_METHOD("debug_get_draw_flag", "flag"), &VoxelInstancer::debug_get_draw_flag);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE,
						 VoxelInstanceLibrary::get_class_static()),
			"set_library", "get_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "up_mode", PROPERTY_HINT_ENUM, "PositiveY,Sphere"), "set_up_mode",
			"get_up_mode");

	BIND_CONSTANT(MAX_LOD);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);

	BIND_ENUM_CONSTANT(DEBUG_DRAW_ALL_BLOCKS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_EDITED_BLOCKS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_FLAGS_COUNT);
}

} // namespace zylann::voxel
