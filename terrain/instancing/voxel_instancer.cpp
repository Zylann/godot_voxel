#include "../../constants/voxel_string_names.h"
#include "../../edition/voxel_tool.h"
#include "../../engine/buffered_task_scheduler.h"
#include "../../streams/save_block_data_task.h"
#include "../../util/containers/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/collision_shape_3d.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/multimesh.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/ref_counted.h"
#include "../../util/godot/classes/resource_saver.h"
#include "../../util/godot/classes/time.h"
#include "../../util/godot/classes/viewport.h"
#include "../../util/godot/core/aabb.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/basis.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string/format.h"
#include "../fixed_lod/voxel_terrain.h"
#include "../variable_lod/voxel_lod_terrain.h"
#include "instancer_quick_reloading_cache.h"
#include "load_instance_block_task.h"
#include "voxel_instance_component.h"
#include "voxel_instance_generator.h"
#include "voxel_instance_library_multimesh_item.h"
#include "voxel_instance_library_scene_item.h"
#include "voxel_instancer_rigidbody.h"

#ifdef TOOLS_ENABLED
#include "../../editor/camera_cache.h"
#include "../../util/godot/core/packed_arrays.h"
#endif

// Only needed for debug purposes, otherwise RenderingServer is used directly
#include "../../util/godot/classes/multimesh_instance_3d.h"

#include <algorithm>

namespace zylann::voxel {

namespace {
StdVector<Transform3f> &get_tls_transform_cache() {
	static thread_local StdVector<Transform3f> tls_transform_cache;
	return tls_transform_cache;
}
} // namespace

VoxelInstancer::VoxelInstancer() {
	set_notify_transform(true);
	set_process_internal(true);
	_loading_results = make_shared_instance<InstancerTaskOutputQueue>();
	fill(_mesh_lod_distances, 0.f);
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
			remove_block(i, false);
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
				update_mesh_lod_distances_from_parent();
				vlt->set_instancer(this);
			} else {
				VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(get_parent());
				if (vt != nullptr) {
					_parent = vt;
					_parent_data_block_size_po2 = vt->get_data_block_size_pow2();
					_parent_mesh_block_size_po2 = vt->get_mesh_block_size_pow2();
					update_mesh_lod_distances_from_parent();
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
	ZN_PROFILE_SCOPE();

	process_task_results();

	if (_parent != nullptr) {
		if (_library.is_valid()) {
			if (_mesh_lod_distances[0] > 0.f) {
				process_mesh_lods();
			}
			process_collision_distances();
		}

#ifdef TOOLS_ENABLED
		if (_gizmos_enabled && is_visible_in_tree()) {
			process_gizmos();
		}
#endif
	}

	process_fading();
}

void VoxelInstancer::process_task_results() {
	ZN_PROFILE_SCOPE();
	static thread_local StdVector<InstanceLoadingTaskOutput> tls_results;
	StdVector<InstanceLoadingTaskOutput> &results = tls_results;
#ifdef DEBUG_ENABLED
	if (results.size()) {
		ZN_PRINT_ERROR("Results were not cleaned up?");
	}
#endif
	{
		MutexLock mlock(_loading_results->mutex);
		// Copy results to temporary buffer
		StdVector<InstanceLoadingTaskOutput> &src = _loading_results->results;
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
	ZN_ASSERT_RETURN(maybe_world.is_valid());
	World3D &world = **maybe_world;

	const Transform3D parent_transform = get_global_transform();

	const int data_block_size_base = (1 << _parent_mesh_block_size_po2);
	const int mesh_block_size_base = (1 << _parent_mesh_block_size_po2);
	const int render_to_data_factor = mesh_block_size_base / data_block_size_base;

	for (InstanceLoadingTaskOutput &output : results) {
		auto layer_it = _layers.find(output.layer_id);
		if (layer_it == _layers.end()) {
			// Layer was removed since?
			ZN_PRINT_VERBOSE(
					format("Processing async instance generator results, but the layer isn't present ({}).",
						   static_cast<int>(output.layer_id))
			);
			continue;
		}
		Layer &layer = layer_it->second;

		const VoxelInstanceLibraryItem *item = _library->get_item(output.layer_id);
		ZN_ASSERT_CONTINUE_MSG(item != nullptr, "Item removed from library while it was loading?");

		auto block_it = layer.blocks.find(output.render_block_position);
		if (block_it == layer.blocks.end()) {
			// The block was removed while the generation process was running?
			ZN_PRINT_VERBOSE("Processing async instance generator results, but the block was removed.");
			continue;
		}

		if (output.edited_mask != 0) {
			Lod &lod = _lods[layer.lod_index];
			const Vector3i minp = output.render_block_position * render_to_data_factor;
			const Vector3i maxp = minp + Vector3iUtil::create(render_to_data_factor);
			Vector3i bpos;
			unsigned int i = 0;
			for (bpos.z = minp.z; bpos.z < maxp.z; ++bpos.z) {
				for (bpos.y = minp.y; bpos.y < maxp.y; ++bpos.y) {
					for (bpos.x = minp.x; bpos.x < maxp.x; ++bpos.x) {
						if ((output.edited_mask & (1 << i)) != 0) {
							lod.edited_data_blocks.insert(bpos);
						}
						++i;
					}
				}
			}
		}

		const int mesh_block_size = mesh_block_size_base << layer.lod_index;
		const Transform3D block_local_transform = Transform3D(Basis(), output.render_block_position * mesh_block_size);
		const Transform3D block_global_transform = parent_transform * block_local_transform;

		update_block_from_transforms(
				block_it->second,
				to_span_const(output.transforms),
				output.render_block_position,
				layer,
				*item,
				output.layer_id,
				world,
				block_global_transform,
				block_local_transform.origin
		);

		if (_fading_enabled) {
			const VoxelInstanceLibraryMultiMeshItem *mm_item =
					Object::cast_to<const VoxelInstanceLibraryMultiMeshItem>(item);

			if (mm_item != nullptr) {
				FadingInBlock fb;
				fb.grid_position = output.render_block_position;
				fb.layer_id = output.layer_id;
				_fading_in_blocks.push_back(fb);
			}
		}
	}

	results.clear();
}

#ifdef TOOLS_ENABLED

void VoxelInstancer::process_gizmos() {
	ZN_PROFILE_SCOPE();

	using namespace zylann::godot;

	struct L {
		static inline void draw_box(
				DebugRenderer &dr,
				const Transform3D parent_transform,
				Vector3i bpos,
				unsigned int lod_index,
				unsigned int base_block_size_po2,
				Color8 color
		) {
			const int block_size_po2 = base_block_size_po2 + lod_index;
			const int block_size = 1 << block_size_po2;
			const Vector3 block_local_pos(bpos << block_size_po2);
			const Transform3D box_transform(
					parent_transform.basis * (Basis().scaled(Vector3(block_size, block_size, block_size))),
					parent_transform.xform(block_local_pos)
			);
			dr.draw_box(box_transform, color);
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

			L::draw_box(
					_debug_renderer,
					parent_transform,
					block.grid_position,
					block.lod_index,
					base_mesh_block_size_po2,
					color
			);
		}
	}

	if (debug_get_draw_flag(DEBUG_DRAW_EDITED_BLOCKS)) {
		for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
			const Lod &lod = _lods[lod_index];

			const Color8 edited_color(0, 255, 0, 255);
			const Color8 unsaved_color(255, 255, 0, 255);

			for (auto it = lod.edited_data_blocks.begin(); it != lod.edited_data_blocks.end(); ++it) {
				L::draw_box(_debug_renderer, parent_transform, *it, lod_index, base_data_block_size_po2, edited_color);
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

namespace {
Vector3 get_global_camera_position(const Node &node) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return zylann::voxel::godot::get_3d_editor_camera_position();
	}
#endif
	const Viewport *viewport = node.get_viewport();
	ZN_ASSERT_RETURN_V(viewport != nullptr, Vector3());
	const Camera3D *camera = viewport->get_camera_3d();
	if (camera == nullptr) {
		return Vector3();
	}
	return camera->get_global_position();
}

} // namespace

void VoxelInstancer::update_mesh_from_mesh_lod(
		Block &block,
		const VoxelInstanceLibraryMultiMeshItem::Settings &settings,
		bool hide_beyond_max_lod,
		bool instancer_is_visible
) {
	if (hide_beyond_max_lod && block.current_mesh_lod == settings.mesh_lod_count) {
		// Godot doesn't like null meshes, so we have to implement a different code path

		// Can be invalid if there is currently no instance in this block
		if (block.multimesh_instance.is_valid()) {
			block.multimesh_instance.set_visible(false);
		}

	} else {
		Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
		if (multimesh.is_valid()) {
			block.multimesh_instance.set_visible(instancer_is_visible);
			ZN_PROFILE_SCOPE();
			multimesh->set_mesh(settings.mesh_lods[block.current_mesh_lod]);
		}
	}
}

void VoxelInstancer::update_mesh_lod_distances_from_parent() {
	ZN_ASSERT_RETURN(_parent != nullptr);

	VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_parent);
	if (vlt != nullptr) {
		vlt->get_lod_distances(to_span(_mesh_lod_distances));
		return;
	}

	VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(_parent);
	if (vt != nullptr) {
		_mesh_lod_distances[0] = vt->get_max_view_distance();
	}
}

void VoxelInstancer::process_mesh_lods() {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_library.is_null());

	// Note, this form of LOD must be visual only. It supports only one camera.

	// Get viewer position
	const Transform3D gtrans = get_global_transform();
	const Vector3 cam_pos_global = get_global_camera_position(*this);
	const Vector3 cam_pos_local = gtrans.affine_inverse().xform(cam_pos_global);

	ERR_FAIL_COND(_parent == nullptr);
	const unsigned int block_size = 1 << _parent_mesh_block_size_po2;

	const float hysteresis = 1.05;

	const uint64_t time_up_time = Time::get_singleton()->get_ticks_usec() + _mesh_lod_update_budget_microseconds;

	const bool instancer_is_visible = is_visible_in_tree();

	// const unsigned int initial_mesh_lod_time_sliced_block_index = _mesh_lod_time_sliced_block_index;

	while (_mesh_lod_time_sliced_block_index < _blocks.size()) {
		// Iterate a portion of blocks, then check timing budget once after that
		const unsigned int desired_portion_size = 64;
		const unsigned int portion_end = math::min(
				_mesh_lod_time_sliced_block_index + desired_portion_size, static_cast<unsigned int>(_blocks.size())
		);
		Span<UniquePtr<Block>> blocks_portion = to_span_from_position_and_size(
				_blocks, _mesh_lod_time_sliced_block_index, portion_end - _mesh_lod_time_sliced_block_index
		);
		_mesh_lod_time_sliced_block_index = portion_end;

		for (UniquePtr<Block> &block_ptr : blocks_portion) {
			Block &block = *block_ptr;
			// Early exit for empty blocks (we only do this for multimeshes so no need to check other things)
			if (!block.multimesh_instance.is_valid()) {
				continue;
			}

			const VoxelInstanceLibraryItem *item_base = _library->get_item_const(block.layer_id);
			ERR_CONTINUE(item_base == nullptr);
			// TODO Optimization: would be nice to not need this cast by iterating only the same item types
			const VoxelInstanceLibraryMultiMeshItem *item =
					Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item_base);
			if (item == nullptr) {
				// Not a multimesh item
				continue;
			}
			const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();
			const bool hide_beyond_max_lod = item->get_hide_beyond_max_lod();
			const unsigned int extended_mesh_lod_count = settings.mesh_lod_count + (hide_beyond_max_lod ? 1 : 0);
			// Note, "hide beyond max lod" counts as having an extra LOD where the mesh is hidden. So an item can have
			// only one mesh setup, yet be considered having LOD
			if (extended_mesh_lod_count <= 1) {
				// This block has no LOD
				// TODO Optimization: would be nice to not need this conditional by iterating only item types that
				// define lods
				continue;
			}

			const int lod_index = item->get_lod_index();

			Span<const float> distance_ratios = item->get_mesh_lod_distance_ratios();
			const float max_distance = _mesh_lod_distances[lod_index];

			// #ifdef DEBUG_ENABLED
			// 		ERR_FAIL_COND(mesh_lod_count < VoxelInstanceLibraryMultiMeshItem::MAX_MESH_LODS);
			// #endif

			// const Lod &lod = _lods[lod_index];

			const int lod_block_size = block_size << lod_index;
			const int hs = lod_block_size >> 1;
			const Vector3 block_center_local(block.grid_position * lod_block_size + Vector3i(hs, hs, hs));
			const float distance_squared = cam_pos_local.distance_squared_to(block_center_local);

			// Compute current mesh LOD index (note, block.current_mesh_lod can totally be out of range due to eventual
			// config changes, or even as a way to force an update. This will bring it back in range)
			unsigned int current_mesh_lod = block.current_mesh_lod;
			while (current_mesh_lod + 1 < extended_mesh_lod_count &&
				   distance_squared > math::squared(
											  distance_ratios[current_mesh_lod] *
											  max_distance
											  // Exit distance is slightly higher so it has less chance to oscillate
											  // often when near the threshold
											  * hysteresis
									  )) {
				// Decrease detail
				++current_mesh_lod;
			}
			while (current_mesh_lod > 0 &&
				   (distance_squared < math::squared(distance_ratios[current_mesh_lod - 1] * max_distance)
					// Allow mesh LOD index to go down if count is set lower
					|| current_mesh_lod >= extended_mesh_lod_count)) {
				// Increase detail
				--current_mesh_lod;
			}

			// Apply if it changed
			if (block.current_mesh_lod != current_mesh_lod) {
				block.current_mesh_lod = current_mesh_lod;
				update_mesh_from_mesh_lod(block, settings, hide_beyond_max_lod, instancer_is_visible);
			}
		}

		if (Time::get_singleton()->get_ticks_usec() > time_up_time) {
			break;
		}
	}

	// const int64_t updated_blocks_count = _mesh_lod_time_sliced_block_index -
	// initial_mesh_lod_time_sliced_block_index; const float updated_blocks_ratio = _blocks.size() != 0 ?
	// updated_blocks_count / float(_blocks.size()) : 0; ZN_PROFILE_PLOT("Updated Instancer Blocks Mesh LOD",
	// updated_blocks_ratio);

	// Keep restarting the update every frame for now
	if (_mesh_lod_time_sliced_block_index >= _blocks.size()) {
		_mesh_lod_time_sliced_block_index = 0;
	}
}

void VoxelInstancer::process_collision_distances() {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(_library.is_valid());

	// Godot's physics engine (including Jolt) dies in mysterious ways when users want colliders on items that may
	// appear tens of thousands of times.
	// They don't want to reduce the distance at which they spawn, and instead do that only on colliders, which then
	// appears to fix the issues.

	// Note, this is a client-only process at the moment. In server-authoritative scenarios, this would have to be done
	// for every VoxelViewer, which is more expensive.

	// Get viewer position
	const Transform3D gtrans = get_global_transform();
	const Vector3 cam_pos_global = get_global_camera_position(*this);
	const Vector3 cam_pos_local = gtrans.affine_inverse().xform(cam_pos_global);

	ERR_FAIL_COND(_parent == nullptr);
	const unsigned int block_size = 1 << _parent_mesh_block_size_po2;

	const float hysteresis = 1.05;

	const uint64_t time_up_time =
			Time::get_singleton()->get_ticks_usec() + _collision_distance_update_budget_microseconds;

	// TODO Candidate for temp allocator
	StdVector<Transform3f> transforms;

	while (_collision_distance_time_sliced_block_index < _blocks.size()) {
		// Iterate a portion of blocks, then check timing budget once after that
		const unsigned int desired_portion_size = 64;

		const unsigned int portion_begin = _collision_distance_time_sliced_block_index;
		const unsigned int portion_end =
				math::min(portion_begin + desired_portion_size, static_cast<unsigned int>(_blocks.size()));

		Span<UniquePtr<Block>> blocks_portion =
				to_span_from_position_and_size(_blocks, portion_begin, portion_end - portion_begin);

		_collision_distance_time_sliced_block_index = portion_end;

		for (unsigned int rel_block_index = 0; rel_block_index < blocks_portion.size(); ++rel_block_index) {
			UniquePtr<Block> &block_ptr = blocks_portion[rel_block_index];
			Block &block = *block_ptr;
			// Early exit for empty blocks (we only do this for multimeshes so no need to check other things)
			if (!block.multimesh_instance.is_valid()) {
				continue;
			}

			const VoxelInstanceLibraryItem *item_base = _library->get_item_const(block.layer_id);
			ZN_ASSERT_CONTINUE(item_base != nullptr);
			// TODO Optimization: would be nice to not need this cast by iterating only the same item types
			const VoxelInstanceLibraryMultiMeshItem *item =
					Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item_base);
			if (item == nullptr) {
				// Not a multimesh item
				continue;
			}
			const float collision_distance = item->get_collision_distance();
			if (collision_distance <= 0.f) {
				// Disabled, the item must have a collider at all distances
				continue;
			}

			const int lod_block_size = block_size << block.lod_index;
			const Vector3i block_origin = block.grid_position * lod_block_size;

			const float distance_squared = zylann::distance_squared(
					AABB(Vector3(block_origin), zylann::godot::Vector3Utility::splat(lod_block_size)), cam_pos_local
			);

			if (block.distance_colliders_active) {
				// Exit distance is slightly higher so it has less chance to oscillate
				// often when near the threshold
				if (distance_squared > math::squared(collision_distance * hysteresis)) {
					destroy_multimesh_block_colliders(block);
					block.distance_colliders_active = false;
				}
			} else {
				if (distance_squared < math::squared(collision_distance)) {
					const unsigned int block_index = portion_begin + rel_block_index;

					get_instance_transforms_local(block, transforms);

					update_multimesh_block_colliders(
							block, block_index, item->get_multimesh_settings(), to_span(transforms), block_origin
					);
					block.distance_colliders_active = true;
				}
			}
		}

		if (Time::get_singleton()->get_ticks_usec() > time_up_time) {
			break;
		}
	}

	// Keep restarting the update every frame for now
	if (_collision_distance_time_sliced_block_index >= _blocks.size()) {
		_collision_distance_time_sliced_block_index = 0;
	}
}

void VoxelInstancer::process_fading() {
	ZN_PROFILE_SCOPE();

	const float delta_time = get_process_delta_time();
	const float fading_delta = delta_time / math::max(_fading_duration, 0.0001f);

	const StringName &shader_param_name = VoxelStringNames::get_singleton().u_lod_fade;

	{
		unsigned int i = _fading_in_blocks.size();
		while (i > 0) {
			--i;
			FadingInBlock &fb = _fading_in_blocks[i];
			fb.progress = math::min(fb.progress + fading_delta, 1.f);

			auto layer_it = _layers.find(fb.layer_id);
			if (layer_it != _layers.end()) {
				Layer &layer = layer_it->second;

				auto block_index_it = layer.blocks.find(fb.grid_position);
				if (block_index_it != layer.blocks.end()) {
					const unsigned int block_index = block_index_it->second;

					const UniquePtr<Block> &block_ptr = _blocks[block_index];
#ifdef DEV_ENABLED
					ZN_ASSERT(block_ptr != nullptr);
#endif
					Block &block = *block_ptr.get();
					if (block.multimesh_instance.is_valid()) {
						const Vector2 v(fb.progress, 1.f);
						block.multimesh_instance.set_shader_instance_parameter(shader_param_name, v);
					}
				}
			}

			if (fb.progress >= 1.f) {
				unordered_remove(_fading_in_blocks, i);
			}
		}
	}

	{
		unsigned int i = _fading_out_blocks.size();
		while (i > 0) {
			--i;
			FadingOutBlock &fb = _fading_out_blocks[i];
			fb.progress = math::min(fb.progress + fading_delta, 1.f);
			const Vector2 v(fb.progress, 0.f);
#ifdef DEV_ENABLED
			ZN_ASSERT(fb.multimesh_instance.is_valid());
#endif
			fb.multimesh_instance.set_shader_instance_parameter(shader_param_name, v);

			if (fb.progress >= 1.f) {
				// fb.multimesh_instance.destroy();
				// unordered_remove(_fading_out_blocks, i);
				_fading_out_blocks[i] = std::move(_fading_out_blocks.back());
				_fading_out_blocks.pop_back();
			}
		}
	}
}

// We need to do this ourselves because we don't use nodes for multimeshes
void VoxelInstancer::update_visibility() {
	if (!is_inside_tree()) {
		return;
	}
	const bool instancer_is_visible = is_visible_in_tree();

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;

		if (block.multimesh_instance.is_valid()) {
			bool visible_with_lod = true;
			{
				const VoxelInstanceLibraryItem *item_base = _library->get_item_const(block.layer_id);
				ERR_CONTINUE(item_base == nullptr);
				// TODO Optimization: would be nice to not need this cast by iterating only the same item types
				const VoxelInstanceLibraryMultiMeshItem *item =
						Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item_base);
				if (item != nullptr) {
					const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();
					const bool hide_beyond_max_lod = item->get_hide_beyond_max_lod();
					if (hide_beyond_max_lod) {
						visible_with_lod = block.current_mesh_lod < settings.mesh_lod_count;
					}
				}
			}

			block.multimesh_instance.set_visible(instancer_is_visible && visible_with_lod);
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
	if (_parent != nullptr && is_inside_tree()) {
		for (auto it = _layers.begin(); it != _layers.end(); ++it) {
			regenerate_layer(it->first, false);
		}
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

int VoxelInstancer::get_mesh_lod_update_budget_microseconds() const {
	return _mesh_lod_update_budget_microseconds;
}

void VoxelInstancer::set_mesh_lod_update_budget_microseconds(const int p_micros) {
	_mesh_lod_update_budget_microseconds = math::max(p_micros, 0);
}

int VoxelInstancer::get_collision_update_budget_microseconds() const {
	return _collision_distance_update_budget_microseconds;
}

void VoxelInstancer::set_collision_update_budget_microseconds(const int p_micros) {
	_collision_distance_update_budget_microseconds = math::max(p_micros, 0);
}

void VoxelInstancer::set_fading_enabled(const bool enabled) {
	_fading_enabled = enabled;
}

bool VoxelInstancer::get_fading_enabled() const {
	return _fading_enabled;
}

void VoxelInstancer::set_fading_duration(const float duration) {
	_fading_duration = math::clamp(duration, 0.f, 5.f);
}

float VoxelInstancer::get_fading_duration() const {
	return _fading_duration;
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
		StdVector<Vector3i> positions;

		if (parent_vlt != nullptr) {
			parent_vlt->get_meshed_block_positions_at_lod(layer.lod_index, positions);

		} else if (parent_vt != nullptr) {
			// Only LOD 0 is supported
			if (layer.lod_index == 0) {
				parent_vt->get_meshed_block_positions(positions);
			}
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
			return lod.edited_data_blocks.find(pos) != lod.edited_data_blocks.end();
		}

		static inline void extract_octant_transforms(
				const Block &render_block,
				StdVector<Transform3f> &dst,
				uint8_t octant_mask,
				int render_block_size
		) {
			if (!render_block.multimesh_instance.is_valid()) {
				return;
			}
			Ref<MultiMesh> multimesh = render_block.multimesh_instance.get_multimesh();
			ERR_FAIL_COND(multimesh.is_null());
			const int instance_count = zylann::godot::get_visible_instance_count(**multimesh);
			const float h = render_block_size / 2;
			for (int i = 0; i < instance_count; ++i) {
				// TODO Optimize: This is very slow the first time, and there is overhead even after that.
				const Transform3D t = multimesh->get_instance_transform(i);
				const uint8_t octant_index = VoxelInstanceGenerator::get_octant_index(to_vec3f(t.origin), h);
				if ((octant_mask & (1 << octant_index)) != 0) {
					dst.push_back(to_transform3f(t));
				}
			}
		}
	};

	Ref<VoxelGenerator> voxel_generator = _parent->get_generator();

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

		StdVector<Transform3f> &transform_cache = get_tls_transform_cache();
		transform_cache.clear();

		Array surface_arrays;
		int32_t vertex_range_end = -1;
		int32_t index_range_end = -1;
		if (parent_vlt != nullptr) {
			surface_arrays = parent_vlt->get_mesh_block_surface(
					block.grid_position, lod_index, vertex_range_end, index_range_end
			);
		} else if (parent_vt != nullptr) {
			surface_arrays = parent_vt->get_mesh_block_surface(block.grid_position);
		}

		const int mesh_block_size = 1 << _parent_mesh_block_size_po2;
		const int lod_block_size = mesh_block_size << lod_index;

		item->get_generator()->generate_transforms(
				transform_cache,
				block.grid_position,
				block.lod_index,
				layer_id,
				surface_arrays,
				vertex_range_end,
				index_range_end,
				_up_mode,
				octant_mask,
				lod_block_size,
				voxel_generator
		);

		if (render_to_data_factor == 2 && octant_mask != 0xff) {
			// Complete transforms with edited ones
			L::extract_octant_transforms(block, transform_cache, ~octant_mask, mesh_block_size);
			// TODO What if these blocks had loaded data which wasn't yet uploaded for render?
			// We may setup a local transform list as well since it's expensive to get it from VisualServer
		}

		const Transform3D block_local_transform(Basis(), Vector3(block.grid_position * lod_block_size));
		const Transform3D block_transform = parent_transform * block_local_transform;

		update_block_from_transforms(
				block_index,
				to_span_const(transform_cache),
				block.grid_position,
				layer,
				**item,
				layer_id,
				world,
				block_transform,
				block_local_transform.origin
		);
	}
}

void VoxelInstancer::update_layer_meshes(int layer_id) {
	Ref<VoxelInstanceLibraryItem> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	// This method is expected to run on a multimesh layer
	VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);

	const bool hide_beyond_max_lod = item->get_hide_beyond_max_lod();

	const bool instancer_is_visible = is_inside_tree() && is_visible_in_tree();

	const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item->get_multimesh_settings();
	const unsigned int extended_mesh_lod_count = settings.mesh_lod_count + (hide_beyond_max_lod ? 1 : 0);

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		Block &block = **it;
		if (block.layer_id != layer_id || !block.multimesh_instance.is_valid()) {
			continue;
		}
		block.multimesh_instance.set_render_layer(settings.render_layer);
		block.multimesh_instance.set_material_override(settings.material_override);
		block.multimesh_instance.set_cast_shadows_setting(settings.shadow_casting_setting);
		block.multimesh_instance.set_gi_mode(settings.gi_mode);

		block.current_mesh_lod = math::min(static_cast<unsigned int>(block.current_mesh_lod), extended_mesh_lod_count);
		update_mesh_from_mesh_lod(block, settings, hide_beyond_max_lod, instancer_is_visible);
	}
}

void VoxelInstancer::update_layer_scenes(int layer_id) {
	Ref<VoxelInstanceLibraryItem> item_base = _library->get_item(layer_id);
	ERR_FAIL_COND(item_base.is_null());
	// This method is expected to run on a scene layer
	VoxelInstanceLibrarySceneItem *item = Object::cast_to<VoxelInstanceLibrarySceneItem>(*item_base);
	ERR_FAIL_COND(item == nullptr);
	const int data_block_size_po2 = _parent_data_block_size_po2;

	for (unsigned int block_index = 0; block_index < _blocks.size(); ++block_index) {
		Block &block = *_blocks[block_index];

		for (unsigned int instance_index = 0; instance_index < block.scene_instances.size(); ++instance_index) {
			SceneInstance prev_instance = block.scene_instances[instance_index];
			ERR_CONTINUE(prev_instance.root == nullptr);
			SceneInstance instance = create_scene_instance(
					*item, instance_index, block_index, prev_instance.root->get_transform(), data_block_size_po2
			);
			ERR_CONTINUE(instance.root == nullptr);
			block.scene_instances[instance_index] = instance;
			// We just drop the instance without saving, because this function is supposed to occur only in editor,
			// or in the very rare cases where library is modified in game (which would invalidate saves anyways).
			prev_instance.root->queue_free();
		}
	}
}

void VoxelInstancer::on_library_item_changed(int item_id, IInstanceLibraryItemListener::ChangeType change) {
	ERR_FAIL_COND(_library.is_null());

	// TODO It's unclear yet if some code paths do the right thing in case instances got edited

	// This callback will fire after the library was loaded, so most of the time in the editor when the user changes
	// things. If it happens in-game, it might cause performance issues. If so, it's better to configure the library
	// before assigning it to the instancer.

	switch (change) {
		case IInstanceLibraryItemListener::CHANGE_ADDED: {
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());
			add_layer(item_id, item->get_lod_index());
			// In the editor, if you delete a VoxelInstancer, Godot doesn't actually delete it. Instead, it removes it
			// from the scene tree and keeps it around in the UndoRedo history. But the node still receives
			// notifications when the library gets modified... this leads to several issues:
			// - Errors because the node needs to have access to World3D to update
			// - In theory we could not require World3D, but then it still means a lot of processing has to occur to
			// re-generate layers, which is wasted CPU for a node that isn't active or is "currently" deleted by the
			// user.
			// So we stop it from re-generating layers while in that state. I'm not sure to which extent we should
			// be supporting out-of-tree automatic refresh... There might be more corner cases than this.
			if (is_inside_tree()) {
				regenerate_layer(item_id, true);
			}
			update_configuration_warnings();
		} break;

		case IInstanceLibraryItemListener::CHANGE_REMOVED:
			remove_layer(item_id);
			update_configuration_warnings();
			break;

		case IInstanceLibraryItemListener::CHANGE_GENERATOR:
			// Don't update in case the node was deleted in the editor...
			if (is_inside_tree()) {
				regenerate_layer(item_id, false);
			}
			break;

		case IInstanceLibraryItemListener::CHANGE_VISUAL:
			update_layer_meshes(item_id);
			break;

		case IInstanceLibraryItemListener::CHANGE_SCENE:
			update_layer_scenes(item_id);
			break;

		case IInstanceLibraryItemListener::CHANGE_LOD_INDEX: {
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(item_id);
			ERR_FAIL_COND(item.is_null());

			clear_blocks_in_layer(item_id);

			Layer &layer = get_layer(item_id);

			Lod &prev_lod = _lods[layer.lod_index];
			unordered_remove_value(prev_lod.layers, item_id);

			layer.lod_index = item->get_lod_index();

			Lod &new_lod = _lods[layer.lod_index];
			new_lod.layers.push_back(item_id);

			// Don't update in case the node was deleted in the editor...
			if (is_inside_tree()) {
				regenerate_layer(item_id, true);
			}
		} break;

		default:
			ERR_PRINT("Unknown change");
			break;
	}

	update_configuration_warnings();
}

void VoxelInstancer::add_layer(int layer_id, int lod_index) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(lod_index < 0 || lod_index >= MAX_LOD);
	ERR_FAIL_COND_MSG(_layers.find(layer_id) != _layers.end(), "Trying to add a layer that already exists");
#endif

	Lod &lod = _lods[lod_index];

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_MSG(
			std::find(lod.layers.begin(), lod.layers.end(), layer_id) != lod.layers.end(),
			"Layer already referenced by this LOD"
	);
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

void VoxelInstancer::destroy_multimesh_block_colliders(Block &block) {
	ZN_PROFILE_SCOPE();
	for (unsigned int i = 0; i < block.bodies.size(); ++i) {
		VoxelInstancerRigidBody *body = block.bodies[i];
		body->detach_and_destroy();
	}
	block.bodies.clear();
}

void VoxelInstancer::remove_block(const unsigned int block_index, const bool with_fade_out) {
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

	destroy_multimesh_block_colliders(*block);

	for (unsigned int i = 0; i < block->scene_instances.size(); ++i) {
		SceneInstance instance = block->scene_instances[i];
		ERR_CONTINUE(instance.component == nullptr);
		instance.component->detach();
		ERR_CONTINUE(instance.root == nullptr);
		instance.root->queue_free();
	}

	// Update block index references, since we had to change the index of the last block during swap-remove.
	// If the block we removed was actually the last one, we don't enter here
	if (block.get() != &moved_block) {
		// Update the index of the moved block referenced in its layer
		Layer &layer = get_layer(moved_block.layer_id);
		auto it = layer.blocks.find(moved_block.grid_position);
		CRASH_COND(it == layer.blocks.end());
		it->second = block_index;

		for (VoxelInstancerRigidBody *body : moved_block.bodies) {
			body->set_render_block_index(block_index);
		}
		for (const SceneInstance &scene : moved_block.scene_instances) {
			scene.component->set_render_block_index(block_index);
		}
	}

	if (with_fade_out && block->multimesh_instance.is_valid()) {
		FadingOutBlock fb;
		fb.multimesh_instance = std::move(block->multimesh_instance);
		_fading_out_blocks.push_back(std::move(fb));
	}
}

// void VoxelInstancer::on_data_block_loaded(
// 		Vector3i grid_position, unsigned int lod_index, UniquePtr<InstanceBlockData> instances) {
// 	ERR_FAIL_COND(lod_index >= _lods.size());
// 	Lod &lod = _lods[lod_index];
// 	lod.loaded_instances_data.insert(std::make_pair(grid_position, std::move(instances)));
// }

void VoxelInstancer::on_mesh_block_enter(
		const Vector3i render_grid_position,
		const unsigned int lod_index,
		Array surface_arrays,
		const int32_t vertex_range_end,
		const int32_t index_range_end
) {
	if (lod_index >= _lods.size()) {
		return;
	}
	create_render_blocks(render_grid_position, lod_index, surface_arrays, vertex_range_end, index_range_end);
}

void VoxelInstancer::on_mesh_block_exit(const Vector3i render_grid_position, const unsigned int lod_index) {
	if (lod_index >= _lods.size()) {
		// The instancer doesn't handle large LODs
		return;
	}

	ZN_PROFILE_SCOPE();

	Lod &lod = _lods[lod_index];

	BufferedTaskScheduler &scheduler = BufferedTaskScheduler::get_for_current_thread();

	const bool can_save = _parent != nullptr && _parent->get_stream().is_valid();

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
				lod.edited_data_blocks.erase(data_grid_pos);

				auto modified_block_it = lod.modified_blocks.find(data_grid_pos);
				if (modified_block_it != lod.modified_blocks.end()) {
					if (can_save) {
						SaveBlockDataTask *task = save_block(data_grid_pos, lod_index, nullptr, false, true);
						if (task != nullptr) {
							scheduler.push_io_task(task);
						}
					}
					lod.modified_blocks.erase(modified_block_it);
				}
			}
		}
	}

	scheduler.flush();

	// Remove render blocks
	for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
		const int layer_id = *layer_it;

		Layer &layer = get_layer(layer_id);

		auto block_it = layer.blocks.find(render_grid_position);
		if (block_it != layer.blocks.end()) {
			remove_block(block_it->second, _fading_enabled);
		}
	}
}

void VoxelInstancer::save_all_modified_blocks(
		BufferedTaskScheduler &tasks,
		std::shared_ptr<AsyncDependencyTracker> tracker,
		bool with_flush
) {
	ZN_DSTACK();

	ZN_ASSERT_RETURN(_parent != nullptr);
	const bool can_save = _parent->get_stream().is_valid();
	ZN_ASSERT_RETURN_MSG(
			can_save,
			format("Cannot save instances, the parent {} has no {} assigned.",
				   _parent->get_class(),
				   String(VoxelStream::get_class_static()))
	);

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];
		for (auto it = lod.modified_blocks.begin(); it != lod.modified_blocks.end(); ++it) {
			SaveBlockDataTask *task = save_block(*it, lod_index, tracker, with_flush, false);
			if (task != nullptr) {
				tasks.push_io_task(task);
			}
		}
		lod.modified_blocks.clear();
	}
}

void VoxelInstancer::remove_instances_in_sphere(const Vector3 p_center, const float p_radius) {
	ZN_PROFILE_MESSAGE("RemoveInSphere");

	class RemoveInSphere : public IAreaOperation {
	private:
		VoxelInstancer &_instancer;
		const Vector3 _center;
		const float _radius_squared;
		MMRemovalAction _mm_removal_action;
		uint32_t _mm_removal_action_item_id;

	public:
		RemoveInSphere(VoxelInstancer &pp_instancer, const Vector3 pp_center, const float pp_radius) :
				_instancer(pp_instancer),
				_center(pp_center),
				_radius_squared(pp_radius * pp_radius),
				_mm_removal_action_item_id(VoxelInstanceLibrary::MAX_ID) //
		{}

		Result execute(Block &block) override {
			ZN_PROFILE_SCOPE();

			const unsigned int base_block_size = 1 << _instancer._parent_mesh_block_size_po2;
			const Vector3 block_origin = Vector3i(block.grid_position * (base_block_size << block.lod_index));
			const Vector3f center_local = to_vec3f(_center - block_origin);

			// TODO Candidate for temp allocator
			// TODO If we had our own cache, we might not need to allocate at all
			StdVector<Vector3f> instance_positions;
			get_instance_positions_local(block, base_block_size, instance_positions, nullptr);

			// TODO Candidate for temp allocator
			StdVector<uint32_t> instances_to_remove;
			unsigned int instance_index = 0;
			for (const Vector3f &instance_pos : instance_positions) {
				const float ds = math::distance_squared(instance_pos, center_local);
				if (ds < _radius_squared) {
					instances_to_remove.push_back(instance_index);
				}
				++instance_index;
			}

			if (instances_to_remove.size() == 0) {
				return { false };
			}

			try_update_item_cache(block.layer_id);

			remove_instances_by_index(block, base_block_size, to_span(instances_to_remove), _mm_removal_action);

			return { true };
		}

	private:
		void try_update_item_cache(const uint32_t item_id) {
			if (item_id == _mm_removal_action_item_id) {
				return;
			}

			// Re-cache item info

			_mm_removal_action = MMRemovalAction();
			_mm_removal_action_item_id = item_id;

			if (_instancer._library.is_null()) {
				return;
			}
			VoxelInstanceLibraryItem *item = _instancer._library->get_item(item_id);
			if (item == nullptr) {
				return;
			}
			VoxelInstanceLibraryMultiMeshItem *mm_item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item);
			if (mm_item == nullptr) {
				return;
			}
			_mm_removal_action = get_mm_removal_action(&_instancer, mm_item);
		}
	};

	RemoveInSphere op(*this, p_center, p_radius);

	const Vector3 rv(p_radius, p_radius, p_radius);
	do_area_operation(AABB(p_center - rv, 2 * rv), op);
}

VoxelInstancer::SceneInstance VoxelInstancer::create_scene_instance(
		const VoxelInstanceLibrarySceneItem &scene_item,
		int instance_index,
		unsigned int block_index,
		Transform3D transform,
		int data_block_size_po2
) {
	SceneInstance instance;
	ERR_FAIL_COND_V_MSG(
			scene_item.get_scene().is_null(),
			instance,
			String("{0} ({1}) is missing an attached scene in {2} ({3})")
					.format(
							varray(VoxelInstanceLibrarySceneItem::get_class_static(),
								   scene_item.get_item_name(),
								   VoxelInstancer::get_class_static(),
								   get_path())
					)
	);
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
		Layer &layer,
		const uint16_t layer_id,
		const Vector3i grid_position,
		const bool pending_instances
) {
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

void VoxelInstancer::update_block_from_transforms(
		int block_index,
		Span<const Transform3f> transforms,
		const Vector3i grid_position,
		Layer &layer,
		const VoxelInstanceLibraryItem &item_base,
		const uint16_t layer_id,
		World3D &world,
		const Transform3D &block_global_transform,
		const Vector3 block_local_position
) {
	ZN_PROFILE_SCOPE();

	// Get or create block
	if (block_index == -1) {
		block_index = create_block(layer, layer_id, grid_position, false);
	}
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(block_index < 0 || block_index >= static_cast<int>(_blocks.size()));
	ERR_FAIL_COND(_blocks[block_index] == nullptr);
#endif
	Block &block = *_blocks[block_index];

	// Update multimesh
	const VoxelInstanceLibraryMultiMeshItem *item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(&item_base);
	if (item != nullptr) {
		update_multimesh_block_from_transforms(
				block, block_index, block_global_transform, block_local_position, transforms, *item, world
		);
		return;
	}

	// Update scene instances
	const VoxelInstanceLibrarySceneItem *scene_item = Object::cast_to<VoxelInstanceLibrarySceneItem>(&item_base);
	if (scene_item != nullptr) {
		update_scene_block_from_transforms(block, block_index, block_local_position, transforms, *scene_item);
	}
}

void VoxelInstancer::update_multimesh_block_from_transforms(
		Block &block,
		const unsigned int block_index,
		const Transform3D &block_global_transform,
		const Vector3 block_local_position,
		Span<const Transform3f> transforms,
		const VoxelInstanceLibraryMultiMeshItem &item,
		World3D &world
) {
	ZN_PROFILE_SCOPE();

	const VoxelInstanceLibraryMultiMeshItem::Settings &settings = item.get_multimesh_settings();

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
		zylann::godot::DirectMultiMeshInstance::make_transform_3d_bulk_array(transforms, bulk_array);
		multimesh->set_instance_count(transforms.size());

		// Setting the mesh BEFORE `multimesh_set_buffer` because otherwise Godot computes the AABB inside
		// `multimesh_set_buffer` BY DOWNLOADING BACK THE BUFFER FROM THE GRAPHICS CARD which can incur a very harsh
		// performance penalty
		// TODO If we could use custom AABBs, we would not need this reordering
		if (settings.mesh_lod_count > 0) {
			if (block.current_mesh_lod < settings.mesh_lod_count) {
				multimesh->set_mesh(settings.mesh_lods[block.current_mesh_lod]);
			}
		}

		// TODO Waiting for Godot to expose the method on the resource object
		// multimesh->set_as_bulk_array(bulk_array);
		RenderingServer::get_singleton()->multimesh_set_buffer(multimesh->get_rid(), bulk_array);

		if (!block.multimesh_instance.is_valid()) {
			block.multimesh_instance.create();
			block.multimesh_instance.set_interpolated(false);
			block.multimesh_instance.set_visible(
					is_visible() &&
					!(item.get_hide_beyond_max_lod() && block.current_mesh_lod == settings.mesh_lod_count)
			);
		}
		block.multimesh_instance.set_multimesh(multimesh);
		block.multimesh_instance.set_render_layer(settings.render_layer);
		block.multimesh_instance.set_world(&world);
		block.multimesh_instance.set_transform(block_global_transform);
		block.multimesh_instance.set_material_override(settings.material_override);
		block.multimesh_instance.set_cast_shadows_setting(settings.shadow_casting_setting);
		block.multimesh_instance.set_gi_mode(settings.gi_mode);

		if (settings.mesh_lod_count > 1 || (settings.mesh_lod_count == 1 && item.get_hide_beyond_max_lod())) {
			// Hide for now, let the LOD system show/hide and assign the right mesh when it runs. We do this because
			// the LOD system doesn't necessarily update every blocks every frame, which would flicker at their full
			// LOD when spawning
			block.current_mesh_lod = settings.mesh_lod_count;
			block.multimesh_instance.set_visible(false);
		}
	}

	if (item.get_collision_distance() < 0.f) {
		// Colliders always present, create them all right away
		update_multimesh_block_colliders(block, block_index, settings, transforms, block_local_position);
	}
}

void VoxelInstancer::update_multimesh_block_colliders(
		Block &block,
		const uint32_t block_index,
		const InstanceLibraryMultiMeshItemSettings &settings,
		Span<const Transform3f> transforms,
		const Vector3 block_local_position
) {
	// Update bodies
	Span<const CollisionShapeInfo> collision_shapes = to_span(settings.collision_shapes);
	if (collision_shapes.size() == 0) {
		return;
	}

	ZN_PROFILE_SCOPE();

	const int data_block_size_po2 = _parent_data_block_size_po2;

	// Add new bodies
	for (unsigned int instance_index = 0; instance_index < transforms.size(); ++instance_index) {
		const Transform3D local_transform = to_transform3(transforms[instance_index]);
		// Bodies are child nodes of the instancer, so we use local block coordinates
		const Transform3D body_transform(local_transform.basis, local_transform.origin + block_local_position);

		VoxelInstancerRigidBody *body;

		if (instance_index < block.bodies.size()) {
			// Body already exists, we'll only update its properties
			body = block.bodies[instance_index];

		} else {
			// Create body

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
			body->set_collision_layer(settings.collision_layer);
			body->set_collision_mask(settings.collision_mask);

			for (unsigned int i = 0; i < collision_shapes.size(); ++i) {
				const CollisionShapeInfo &shape_info = collision_shapes[i];
				CollisionShape3D *cs = memnew(CollisionShape3D);
				cs->set_shape(shape_info.shape);
				cs->set_transform(shape_info.transform);
				body->add_child(cs);
			}

			for (const StringName &group_name : settings.group_names) {
				body->add_to_group(group_name);
			}

			add_child(body);
			block.bodies.push_back(body);
		}

		body->set_transform(body_transform);
	}

	// Remove excess bodies
	for (unsigned int instance_index = transforms.size(); instance_index < block.bodies.size(); ++instance_index) {
		VoxelInstancerRigidBody *body = block.bodies[instance_index];
		body->detach_and_destroy();
	}

	block.bodies.resize(transforms.size());
}

void VoxelInstancer::update_scene_block_from_transforms(
		Block &block,
		const unsigned int block_index,
		const Vector3 block_local_position,
		Span<const Transform3f> transforms,
		const VoxelInstanceLibrarySceneItem &scene_item
) {
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND_MSG(
			scene_item.get_scene().is_null(),
			String("{0} ({1}) is missing an attached scene in {2} ({3})")
					.format(
							varray(VoxelInstanceLibrarySceneItem::get_class_static(),
								   scene_item.get_item_name(),
								   VoxelInstancer::get_class_static(),
								   get_path())
					)
	);

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
			instance =
					create_scene_instance(scene_item, instance_index, block_index, body_transform, data_block_size_po2);
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

void VoxelInstancer::create_render_blocks(
		const Vector3i render_grid_position,
		const int lod_index,
		Array surface_arrays,
		const int32_t vertex_range_end,
		const int32_t index_range_end
) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(_library.is_valid());
	ZN_ASSERT_RETURN(_parent != nullptr);
	Ref<VoxelStream> stream = _parent->get_stream();
	Ref<VoxelGenerator> generator = _parent->get_generator();

	const unsigned int render_block_size = 1 << _parent_mesh_block_size_po2;
	const unsigned int data_block_size = 1 << _parent_data_block_size_po2;

	Lod &lod = _lods[lod_index];

	// Create empty blocks in pending state
	for (auto layer_it = lod.layers.begin(); layer_it != lod.layers.end(); ++layer_it) {
		const int layer_id = *layer_it;

		Layer &layer = get_layer(layer_id);

		if (layer.blocks.find(render_grid_position) != layer.blocks.end()) {
			// The block was already made?
			continue;
		}

		create_block(layer, layer_id, render_grid_position, true);
	}

	LoadInstanceChunkTask *task = ZN_NEW(LoadInstanceChunkTask(
			_loading_results,
			stream,
			generator,
			lod.quick_reload_cache,
			_library,
			surface_arrays,
			vertex_range_end,
			index_range_end,
			render_grid_position,
			lod_index,
			render_block_size,
			data_block_size,
			_up_mode
	));

	VoxelEngine::get_singleton().push_async_io_task(task);
}

SaveBlockDataTask *VoxelInstancer::save_block(
		Vector3i data_grid_pos,
		int lod_index,
		std::shared_ptr<AsyncDependencyTracker> tracker,
		bool with_flush,
		bool cache_while_saving
) {
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

			const int instance_count = zylann::godot::get_visible_instance_count(**multimesh);

			if (render_to_data_factor == 1) {
				layer_data.instances.resize(instance_count);

				// TODO Optimization: it would be nice to get the whole array at once
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO Optimize: This is very slow the first time, and there is overhead even after that.
					layer_data.instances[instance_index].transform =
							to_transform3f(multimesh->get_instance_transform(instance_index));
				}

			} else if (render_to_data_factor == 2) {
				for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
					// TODO Optimize: This is terrible in MT mode! Think about keeping a local copy...
					const Transform3D rendered_instance_transform = multimesh->get_instance_transform(instance_index);
					const int instance_octant_index = VoxelInstanceGenerator::get_octant_index(
							to_vec3f(rendered_instance_transform.origin), half_render_block_size
					);
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

	if (cache_while_saving) {
		Lod &lod_mutable = _lods[lod_index];
		// Keep data in memory in case it quickly gets reloaded
		// TODO Making a pre-emptive copy isn't very efficient, we could keep a shared_ptr instead?
		UniquePtr<InstanceBlockData> saving_cache = make_unique_instance<InstanceBlockData>();
		block_data->copy_to(*saving_cache);
		if (lod_mutable.quick_reload_cache == nullptr) {
			lod_mutable.quick_reload_cache = make_shared_instance<InstancerQuickReloadingCache>();
		}
		{
			MutexLock mlock(lod_mutable.quick_reload_cache->mutex);
			lod_mutable.quick_reload_cache->map[data_grid_pos] = std::move(saving_cache);
		}
	}

	SaveBlockDataTask *task = ZN_NEW(SaveBlockDataTask(
			volume_id, data_grid_pos, lod_index, std::move(block_data), stream_dependency, tracker, with_flush
	));

	return task;
}

inline bool detect_ground(
		const Vector3 instance_position_local,
		const Vector3 instance_up,
		const Vector3 block_origin,
		const float sd_threshold,
		const float sd_offset,
		const bool bidirectional,
		const VoxelTool &voxel_tool
) {
	const Vector3 normal_offset = sd_offset * instance_up;
	const Vector3 instance_pos_terrain = instance_position_local + block_origin;
	const Vector3 instance_pos_terrain_below = instance_pos_terrain + normal_offset;

	// TODO Optimize: use a transaction instead of random single queries
	const float sdf_below = voxel_tool.get_voxel_f_interpolated(instance_pos_terrain_below);
	if (sdf_below <= sd_threshold) {
		// Still enough ground
		return true;
	}

	if (bidirectional) {
		// Attempt sampling above instead, in case the instance is flipped over
		const Vector3 instance_pos_terrain_above = instance_pos_terrain - normal_offset;
		const float sdf_above = voxel_tool.get_voxel_f_interpolated(instance_pos_terrain_above);
		if (sdf_above <= sd_threshold) {
			return true;
		}
	}

	return false;
}

VoxelInstancer::MMRemovalAction VoxelInstancer::get_mm_removal_action(
		VoxelInstancer *instancer,
		VoxelInstanceLibraryMultiMeshItem *mm_item
) {
	if (mm_item == nullptr) {
		ZN_PRINT_ERROR_ONCE("Didn't expect multimesh item to be null, bug?");
		return MMRemovalAction();
	}

	switch (mm_item->get_removal_behavior()) {
		case VoxelInstanceLibraryMultiMeshItem::REMOVAL_BEHAVIOR_NONE:
			break;

		case VoxelInstanceLibraryMultiMeshItem::REMOVAL_BEHAVIOR_INSTANTIATE: {
			{
				Ref<PackedScene> scene = mm_item->get_removal_scene();
				if (scene.is_null()) {
#ifdef TOOLS_ENABLED
					Ref<VoxelInstanceLibrary> lib = instancer->get_library();
					const int item_id = lib->get_item_id(mm_item);
					ZN_PRINT_ERROR_ONCE(format(
							"Removal behavior of item {} is set to instantiate a scene, but the scene is null.", item_id
					));
					return MMRemovalAction();
#endif
				}
			}
			MMRemovalAction action;
			action.context = { instancer, mm_item };
			action.callback = [](MMRemovalAction::Context ctx, const Transform3D &trans) {
				Ref<PackedScene> scene = ctx.item->get_removal_scene();
				Node *root = scene->instantiate();
				Node3D *root_3d = Object::cast_to<Node3D>(root);
				if (root_3d != nullptr) {
					root_3d->set_transform(trans);
					// We can't add_child when the callback occurs from within the removal of bodies, because Godot
					// locks children of VoxelInstancer during the process, preventing from adding nodes...
					// ctx.instancer->add_child(root);
					ctx.instancer->call_deferred(VoxelStringNames::get_singleton().add_child, root);
				} else {
#ifdef TOOLS_ENABLED
					Ref<VoxelInstanceLibrary> lib = ctx.instancer->get_library();
					const int item_id = lib->get_item_id(ctx.item);
					ZN_PRINT_ERROR_ONCE(
							format("Removal behavior of item {} is set to instantiate a scene, but its root is not "
								   "a Node3D.",
								   item_id)
					);
#endif
				}
			};
			return action;
		}

		case VoxelInstanceLibraryMultiMeshItem::REMOVAL_BEHAVIOR_CALLBACK: {
			MMRemovalAction action;
			action.context = { instancer, mm_item };
			action.callback = [](MMRemovalAction::Context ctx, const Transform3D &trans) {
				ctx.item->trigger_removal_callback(ctx.instancer, trans);
			};
			return action;
		}

		default:
			ZN_PRINT_ERROR("Unknown removal mode");
			break;
	}

	return MMRemovalAction();
}

void VoxelInstancer::get_instance_positions_local(
		const Block &block,
		const unsigned int base_block_size,
		StdVector<Vector3f> &dst_positions,
		StdVector<Vector3f> *dst_normals
) {
	ZN_PROFILE_SCOPE();

	dst_positions.clear();
	if (dst_normals != nullptr) {
		dst_normals->clear();
	}

	if (block.scene_instances.size() > 0) {
		const Vector3 block_origin(block.grid_position * (base_block_size << block.lod_index));

		dst_positions.reserve(block.scene_instances.size());

		for (const SceneInstance &si : block.scene_instances) {
			const Vector3 pos_terrain = si.root->get_position();
			dst_positions.push_back(to_vec3f(pos_terrain - block_origin));
		}

		if (dst_normals != nullptr) {
			dst_normals->reserve(dst_positions.size());

			for (const SceneInstance &si : block.scene_instances) {
				const Vector3 normal = zylann::godot::BasisUtility::get_up(si.root->get_basis());
				dst_positions.push_back(to_vec3f(normal));
			}
		}

	} else {
		if (!block.multimesh_instance.is_valid()) {
			// Empty block
			return;
		}

		Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
		ZN_ASSERT_RETURN(multimesh.is_valid());

		const unsigned int instance_count = zylann::godot::get_visible_instance_count(**multimesh);
		{
			ZN_PROFILE_SCOPE_NAMED("Alloc P");
			dst_positions.reserve(instance_count);
		}

		if (dst_normals != nullptr) {
			{
				ZN_PROFILE_SCOPE_NAMED("Alloc N");
				dst_normals->reserve(instance_count);
			}

			for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
				// TODO Optimize: This is very slow the first time, and there is overhead even after that.
				//      Would it be better to use `multimesh_get_buffer`? Unfortunately it ALWAYS allocates (it
				//      isn't even benefiting from CoW), and it also doesn't cache, so to force it we'd have to do a
				//      dummy call to `get_instance_transform`. Using our own cache and carefully avoiding Godot
				//      from populating its own is still better...
				const Transform3D instance_transform = multimesh->get_instance_transform(instance_index);
				dst_positions.push_back(to_vec3f(instance_transform.origin));
				dst_normals->push_back(to_vec3f(zylann::godot::BasisUtility::get_up(instance_transform.basis)));
			}
		} else {
			for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
				// TODO Optimize: This is very slow the first time, and there is overhead even after that.
				const Transform3D instance_transform = multimesh->get_instance_transform(instance_index);
				dst_positions.push_back(to_vec3f(instance_transform.origin));
			}
		}
	}
}

void VoxelInstancer::get_instance_transforms_local(const Block &block, StdVector<Transform3f> &dst) {
	ZN_PROFILE_SCOPE();

	Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
	ZN_ASSERT_RETURN(multimesh.is_valid());
	const unsigned int instance_count = zylann::godot::get_visible_instance_count(**multimesh);

	dst.resize(instance_count);

	for (unsigned int instance_index = 0; instance_index < instance_count; ++instance_index) {
		// TODO Optimize: This is very slow the first time, and there is overhead even after that.
		const Transform3D trans = multimesh->get_instance_transform(instance_index);
		dst[instance_index] = to_transform3f(trans);
	}
}

void VoxelInstancer::remove_instances_by_index(
		Block &block,
		const uint32_t base_block_size,
		Span<const uint32_t> ascending_indices,
		const MMRemovalAction mm_removal_action
) {
#ifdef DEV_ENABLED
	for (unsigned int i = 1; i < ascending_indices.size(); ++i) {
		ZN_ASSERT(ascending_indices[i] > ascending_indices[i - 1]);
	}
#endif

	if (block.scene_instances.size() > 0) {
		remove_scene_instances_by_index(block, ascending_indices);

	} else {
		remove_multimesh_instances_by_index(block, base_block_size, ascending_indices, mm_removal_action);
	}
}

void VoxelInstancer::remove_scene_instances_by_index(Block &block, Span<const uint32_t> ascending_indices) {
	ZN_PROFILE_SCOPE();

	const unsigned int initial_instance_count = block.scene_instances.size();
	unsigned int instance_count = initial_instance_count;

	unsigned int removal_list_index = ascending_indices.size();
	while (removal_list_index > 0) {
		--removal_list_index;

		const unsigned int instance_index = ascending_indices[removal_list_index];

		SceneInstance instance = block.scene_instances[instance_index];
		ERR_CONTINUE(instance.root == nullptr);

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
	}

	if (instance_count < initial_instance_count) {
		if (block.scene_instances.size() > 0) {
			block.scene_instances.resize(instance_count);
		}
	}
}

void VoxelInstancer::remove_multimesh_instances_by_index(
		Block &block,
		const uint32_t base_block_size,
		Span<const uint32_t> ascending_indices,
		const MMRemovalAction removal_action
) {
	ZN_PROFILE_SCOPE();

	Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
	ZN_ASSERT_RETURN(multimesh.is_valid());

	const int initial_instance_count = zylann::godot::get_visible_instance_count(**multimesh);
	int instance_count = initial_instance_count;

	const int block_size = base_block_size << block.lod_index;

	unsigned int removal_list_index = ascending_indices.size();
	while (removal_list_index > 0) {
		--removal_list_index;

		const unsigned int instance_index = ascending_indices[removal_list_index];

		Transform3D instance_transform;
		if (removal_action.is_valid()) {
			instance_transform = multimesh->get_instance_transform(instance_index);
		}

		// Remove the MultiMesh instance
		const int last_instance_index = --instance_count;
		// TODO This is terrible in MT mode! Think about keeping a local copy...
		const Transform3D last_trans = multimesh->get_instance_transform(last_instance_index);
		// TODO Also, SETTING transforms internally DOWNLOADS the buffer back to RAM in case it wasn't already,
		// which Godot presumably uses to update the VRAM buffer in regions. But regions it uses are 512 items wide,
		// so given our terrain chunks size we often have less items than that so there is very little benefit
		// compared to uploading the whole buffer. Therefore even if we had our own cache to improve performance on
		// our side while avoiding the *need* for Godot to have its own cache, we get little to no benefit from the
		// Godot side.
		multimesh->set_instance_transform(instance_index, last_trans);

		// Remove the body if this block has some
		// TODO In the case of bodies, we could use an overlap check
		if (block.bodies.size() > 0) {
			VoxelInstancerRigidBody *rb = block.bodies[instance_index];
			// Detach so it won't try to update our instances, we already do it here
			rb->detach_and_destroy();

			// Update the last body index since we did a swap-removal
			VoxelInstancerRigidBody *moved_rb = block.bodies[last_instance_index];
			if (moved_rb != rb) {
				moved_rb->set_instance_index(instance_index);
				block.bodies[instance_index] = moved_rb;
			}
		}

		if (removal_action.is_valid()) {
			const Transform3D trans(
					instance_transform.basis, instance_transform.origin + Vector3(block.grid_position * block_size)
			);
			removal_action.call(trans);
		}
	}

	if (instance_count < initial_instance_count) {
		// According to the docs, set_instance_count() resets the array so we only hide them instead
		multimesh->set_visible_instance_count(instance_count);

		if (block.bodies.size() > 0) {
			block.bodies.resize(instance_count);
		}
	}
}

void VoxelInstancer::do_area_operation(const AABB p_aabb, IAreaOperation &op) {
	do_area_operation(
			Box3i::from_min_max(Vector3i(p_aabb.position.floor()), Vector3i((p_aabb.position + p_aabb.size).ceil())), op
	);
}

void VoxelInstancer::do_area_operation(const Box3i p_voxel_box, IAreaOperation &op) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(_parent == nullptr);
	const int render_block_size = 1 << _parent_mesh_block_size_po2;
	const int data_block_size = 1 << _parent_data_block_size_po2;

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Box3i render_blocks_box = p_voxel_box.downscaled(render_block_size << lod_index);

		bool modified = false;

		for (const int layer_id : lod.layers) {
			const Layer &layer = get_layer(layer_id);
			const StdVector<UniquePtr<Block>> &blocks = _blocks;

			// Iterate blocks intersecting the area
			const Vector3i bmax = render_blocks_box.position + render_blocks_box.size;
			Vector3i block_pos;
			for (block_pos.z = render_blocks_box.position.z; block_pos.z < bmax.z; ++block_pos.z) {
				for (block_pos.x = render_blocks_box.position.x; block_pos.x < bmax.x; ++block_pos.x) {
					for (block_pos.y = render_blocks_box.position.y; block_pos.y < bmax.y; ++block_pos.y) {
						//
						const auto block_it = layer.blocks.find(block_pos);
						if (block_it == layer.blocks.end()) {
							// No instancing block here
							continue;
						}

						Block &block = *blocks[block_it->second];
						const IAreaOperation::Result result = op.execute(block);
						modified = modified | result.modified;
					}
				}
			}
		}

		if (modified) {
			const Box3i data_blocks_box = p_voxel_box.downscaled(data_block_size << lod_index);

			// All instances have to be frozen as edited.
			// TODO Optimization: maybe we can narrow it down per item ID, if that's necessary
			data_blocks_box.for_each_cell([&lod](Vector3i data_block_pos) { //
				lod.modified_blocks.insert(data_block_pos);
			});
		}
	}
}

void VoxelInstancer::on_area_edited(Box3i p_voxel_box) {
	remove_floating_instances(p_voxel_box);
}

#ifdef VOXEL_INSTANCER_USE_SPECIALIZED_FLOATING_INSTANCE_REMOVAL_IMPLEMENTATION

void VoxelInstancer::remove_floating_instances(const Box3i p_voxel_box) {
	ZN_PROFILE_SCOPE();
	ZN_PROFILE_MESSAGE("RemoveFloatingInstances");

	ERR_FAIL_COND(_parent == nullptr);
	const int render_block_size = 1 << _parent_mesh_block_size_po2;
	const int data_block_size = 1 << _parent_data_block_size_po2;

	Ref<VoxelTool> maybe_voxel_tool = _parent->get_voxel_tool();
	ERR_FAIL_COND(maybe_voxel_tool.is_null());
	VoxelTool &voxel_tool = **maybe_voxel_tool;
	voxel_tool.set_channel(VoxelBuffer::CHANNEL_SDF);

	const Transform3D parent_transform = get_global_transform();
	const int base_block_size_po2 = _parent_mesh_block_size_po2;

	for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		if (lod.layers.size() == 0) {
			continue;
		}

		const Box3i render_blocks_box = p_voxel_box.downscaled(render_block_size << lod_index);

		// Remove floating instances
		for (const int layer_id : lod.layers) {
			const Layer &layer = get_layer(layer_id);
			const StdVector<UniquePtr<Block>> &blocks = _blocks;
			const int block_size_po2 = base_block_size_po2 + layer.lod_index;

			VoxelInstanceLibraryItem *item = nullptr;
			VoxelInstanceLibraryMultiMeshItem *mm_item = nullptr;
			bool bidirectional = false;
			float sd_threshold = 0.f;
			float sd_offset = 0.f;

			const Vector3i bmax = render_blocks_box.position + render_blocks_box.size;
			Vector3i block_pos;
			for (block_pos.z = render_blocks_box.position.z; block_pos.z < bmax.z; ++block_pos.z) {
				for (block_pos.x = render_blocks_box.position.x; block_pos.x < bmax.x; ++block_pos.x) {
					for (block_pos.y = render_blocks_box.position.y; block_pos.y < bmax.y; ++block_pos.y) {
						//
						const auto block_it = layer.blocks.find(block_pos);
						if (block_it == layer.blocks.end()) {
							// No instancing block here
							continue;
						}

						ZN_PROFILE_SCOPE_NAMED("Block");

						Block &block = *blocks[block_it->second];

						if (item == nullptr) {
							item = _library->get_item(layer_id);
							sd_threshold = item->get_floating_sdf_threshold();
							sd_offset = item->get_floating_sdf_offset_along_normal();
							Ref<VoxelInstanceGenerator> generator = item->get_generator();
							if (generator.is_valid()) {
								bidirectional = generator->get_random_vertical_flip();
							}
						}

						if (block.scene_instances.size() > 0) {
							remove_floating_scene_instances(
									block,
									parent_transform,
									p_voxel_box,
									voxel_tool,
									block_size_po2,
									sd_threshold,
									sd_offset,
									bidirectional
							);

						} else {
							if (mm_item == nullptr) {
								mm_item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item);
							}

							MMRemovalAction action = get_mm_removal_action(this, mm_item);

							remove_floating_multimesh_instances(
									block,
									parent_transform,
									p_voxel_box,
									voxel_tool,
									block_size_po2,
									sd_threshold,
									sd_offset,
									bidirectional,
									action
							);
						}
					}
				}
			}
		}

		const Box3i data_blocks_box = p_voxel_box.downscaled(data_block_size << lod_index);

		// All instances have to be frozen as edited.
		// Because even if none of them were removed or added, the ground on which they can spawn has
		// changed, and at the moment we don't want unexpected instances to generate when loading back
		// this area.
		data_blocks_box.for_each_cell([&lod](Vector3i data_block_pos) { //
			lod.modified_blocks.insert(data_block_pos);
		});
	}
}

void VoxelInstancer::remove_floating_multimesh_instances(
		Block &block,
		const Transform3D &parent_transform,
		const Box3i p_voxel_box,
		const VoxelTool &voxel_tool,
		const int block_size_po2,
		const float sd_threshold,
		const float sd_offset,
		const bool bidirectional,
		const MMRemovalAction removal_action
) {
	if (!block.multimesh_instance.is_valid()) {
		// Empty block
		return;
	}

	Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
	ERR_FAIL_COND(multimesh.is_null());

	const int initial_instance_count = zylann::godot::get_visible_instance_count(**multimesh);
	int instance_count = initial_instance_count;

	// const Transform3D block_global_transform =
	// 		Transform3D(parent_transform.basis, parent_transform.xform(block.grid_position << block_size_po2));
	const Vector3i block_origin_in_voxels = block.grid_position << block_size_po2;

	// Let's check all instances one by one
	// Note: the fact we have to query VisualServer in and out is pretty bad though.
	// - We probably have to sync with its thread in MT mode
	// - A hashmap RID lookup is performed to check `RID_Owner::id_map`
	for (int instance_index = 0; instance_index < instance_count; ++instance_index) {
		// TODO Optimize: This is terrible in MT mode! Think about keeping a local copy...
		const Transform3D instance_transform = multimesh->get_instance_transform(instance_index);
		const Vector3i voxel_pos(math::floor_to_int(instance_transform.origin) + block_origin_in_voxels);

		if (!p_voxel_box.contains(voxel_pos)) {
			continue;
		}

		if (detect_ground(
					instance_transform.origin,
					zylann::godot::BasisUtility::get_up(instance_transform.basis),
					Vector3(block_origin_in_voxels),
					sd_threshold,
					sd_offset,
					bidirectional,
					voxel_tool
			)) {
			continue;
		}

		// Remove the MultiMesh instance
		const int last_instance_index = --instance_count;
		// TODO Optimize: This is very slow the first time, and there is overhead even after that.
		const Transform3D last_trans = multimesh->get_instance_transform(last_instance_index);
		// TODO Also, SETTING transforms internally DOWNLOADS the buffer back to RAM in case it wasn't already,
		// which Godot presumably uses to update the VRAM buffer in regions. But regions it uses are 512 items wide, so
		// given our terrain chunks size we often have less items than that so there is very little benefit compared to
		// uploading the whole buffer. Therefore even if we had our own cache to improve performance on our side while
		// avoiding the *need* for Godot to have its own cache, we get little to no benefit from the Godot side.
		multimesh->set_instance_transform(instance_index, last_trans);

		// Remove the body if this block has some
		// TODO In the case of bodies, we could use an overlap check
		if (block.bodies.size() > 0) {
			VoxelInstancerRigidBody *rb = block.bodies[instance_index];
			// Detach so it won't try to update our instances, we already do it here
			rb->detach_and_destroy();

			// Update the last body index since we did a swap-removal
			VoxelInstancerRigidBody *moved_rb = block.bodies[last_instance_index];
			if (moved_rb != rb) {
				moved_rb->set_instance_index(instance_index);
				block.bodies[instance_index] = moved_rb;
			}
		}

		--instance_index;

		if (removal_action.is_valid()) {
			const Transform3D trans(
					instance_transform.basis, instance_transform.origin + Vector3(block_origin_in_voxels)
			);
			removal_action.call(trans);
		}

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

void VoxelInstancer::remove_floating_scene_instances(
		Block &block,
		const Transform3D &parent_transform,
		const Box3i p_voxel_box,
		const VoxelTool &voxel_tool,
		const int block_size_po2,
		const float sd_threshold,
		const float sd_offset,
		const bool bidirectional
) {
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

		if (detect_ground(
					scene_transform.origin,
					zylann::godot::BasisUtility::get_up(scene_transform.basis),
					Vector3(), // Little hack, scenes are already in terrain space
					sd_threshold,
					sd_offset,
					bidirectional,
					voxel_tool
			)) {
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

#else // VOXEL_INSTANCER_USE_SPECIALIZED_FLOATING_INSTANCE_REMOVAL_IMPLEMENTATION

void VoxelInstancer::remove_floating_instances(const Box3i voxel_box) {
	ZN_PROFILE_SCOPE();
	ZN_PROFILE_MESSAGE("RemoveFloatingInstances");

	class RemoveFloatingInstances : public IAreaOperation {
	private:
		VoxelInstancer &_instancer;
		const unsigned int _base_block_size;
		VoxelTool &_voxel_tool;
		Box3i _voxel_box;

		MMRemovalAction _mm_removal_action;
		uint32_t _mm_removal_action_item_id = VoxelInstanceLibrary::MAX_ID;
		float _sd_threshold = 0.f;
		float _offset_along_normal = 0.f;
		bool _bidirectional = false;

	public:
		RemoveFloatingInstances(
				VoxelInstancer &instancer,
				const unsigned int base_block_size,
				VoxelTool &p_voxel_tool,
				const Box3i p_voxel_box
		) :
				_instancer(instancer),
				_base_block_size(base_block_size),
				_voxel_tool(p_voxel_tool),
				_voxel_box(p_voxel_box) //
		{}

		Result execute(Block &block) override {
			ZN_PROFILE_SCOPE();

			// TODO Candidate for temp allocator
			// TODO If we had our own cache, we might not need to allocate at all
			StdVector<Vector3f> instance_positions;
			StdVector<Vector3f> instance_normals;
			get_instance_positions_local(block, _base_block_size, instance_positions, &instance_normals);

			try_update_item_cache(block.layer_id);

			// TODO We can get away with local positions if we use something more optimal than individual voxel queries
			const Vector3i block_origin_i = block.grid_position * (_base_block_size << block.lod_index);
			const Vector3 block_origin(block_origin_i);

			const Box3f box_local = Box3f::from_min_max(
					to_vec3f(_voxel_box.position - block_origin_i),
					to_vec3f(_voxel_box.position - block_origin_i + _voxel_box.size)
			);

			// TODO Candidate for temp allocator
			StdVector<uint32_t> instances_to_remove;
			for (unsigned int instance_index = 0; instance_index < instance_positions.size(); ++instance_index) {
				const Vector3f instance_pos = instance_positions[instance_index];
				if (!box_local.contains(instance_pos)) {
					continue;
				}

				if (!detect_ground(
							to_vec3(instance_positions[instance_index]),
							to_vec3(instance_normals[instance_index]),
							block_origin,
							_sd_threshold,
							_offset_along_normal,
							_bidirectional,
							_voxel_tool
					)) {
					instances_to_remove.push_back(instance_index);
				}
			}

			if (instances_to_remove.size() == 0) {
				return { false };
			}

			remove_instances_by_index(block, _base_block_size, to_span(instances_to_remove), _mm_removal_action);

			return { true };
		}

	private:
		void try_update_item_cache(const uint32_t item_id) {
			if (item_id == _mm_removal_action_item_id) {
				return;
			}

			// Re-cache item info

			_mm_removal_action = MMRemovalAction();
			_mm_removal_action_item_id = item_id;
			_sd_threshold = 0.f;
			_offset_along_normal = 0.f;

			if (_instancer._library.is_null()) {
				return;
			}
			VoxelInstanceLibraryItem *item = _instancer._library->get_item(item_id);
			if (item == nullptr) {
				return;
			}
			_sd_threshold = item->get_floating_sdf_threshold();
			_offset_along_normal = item->get_floating_sdf_offset_along_normal();

			Ref<VoxelInstanceGenerator> instance_generator = item->get_generator();
			_bidirectional = instance_generator.is_valid() ? instance_generator->get_random_vertical_flip() : false;

			VoxelInstanceLibraryMultiMeshItem *mm_item = Object::cast_to<VoxelInstanceLibraryMultiMeshItem>(item);
			if (mm_item == nullptr) {
				return;
			}
			_mm_removal_action = get_mm_removal_action(&_instancer, mm_item);
		}
	};

	Ref<VoxelTool> maybe_voxel_tool = _parent->get_voxel_tool();
	ZN_ASSERT_RETURN(maybe_voxel_tool.is_valid());

	RemoveFloatingInstances op(*this, 1 << _parent_mesh_block_size_po2, **maybe_voxel_tool, voxel_box);

	do_area_operation(voxel_box, op);
}

#endif

// This is called if a user destroys or unparents the body node while it's still attached to the ground
void VoxelInstancer::on_body_removed(
		Vector3i data_block_position,
		unsigned int render_block_index,
		unsigned int instance_index
) {
	ZN_PRINT_VERBOSE(format("on_body_removed from block {}", render_block_index));

	Block &block = *_blocks[render_block_index];

	if (instance_index >= block.bodies.size()) {
		int instance_count = -1;
		if (block.multimesh_instance.is_valid()) {
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			instance_count = zylann::godot::get_visible_instance_count(**multimesh);
		}
		ZN_PRINT_ERROR(
				format("Can't remove instance with index {} (bodies: {}, instances: {})",
					   instance_index,
					   block.bodies.size(),
					   instance_count)
		);
		return;
	}

	if (block.multimesh_instance.is_valid()) {
		// Remove the multimesh instance

		Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
		ERR_FAIL_COND(multimesh.is_null());

		{
			Ref<VoxelInstanceLibraryItem> item = _library->get_item(block.layer_id);
			Ref<VoxelInstanceLibraryMultiMeshItem> mm_item = item;
			MMRemovalAction action = get_mm_removal_action(this, mm_item.ptr());
			if (action.is_valid()) {
				// TODO Optimize: This is very slow the first time, and there is overhead even after that.
				const Transform3D ltrans = multimesh->get_instance_transform(instance_index);
				const Vector3i block_origin_in_voxels = data_block_position
						<< (_parent_mesh_block_size_po2 + block.lod_index);
				const Transform3D trans(ltrans.basis, ltrans.origin + Vector3(block_origin_in_voxels));
				action.call(trans);
			}
		}

		int visible_count = zylann::godot::get_visible_instance_count(**multimesh);
		ERR_FAIL_COND(static_cast<int>(instance_index) >= visible_count);

		--visible_count;
		// Swap-remove
		// TODO Optimize: This is very slow the first time, and there is overhead even after that.
		const Transform3D last_trans = multimesh->get_instance_transform(visible_count);
		multimesh->set_instance_transform(instance_index, last_trans);
		multimesh->set_visible_instance_count(visible_count);
	}

	// Unregister the body
	unsigned int body_count = block.bodies.size();
	const unsigned int last_instance_index = --body_count;
	VoxelInstancerRigidBody *moved_body = block.bodies[last_instance_index];
	if (instance_index != last_instance_index) {
		// Update last body index because we did a swap-remove
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
		Vector3i data_block_position,
		unsigned int render_block_index,
		unsigned int instance_index
) {
	Block &block = *_blocks[render_block_index];
	ZN_ASSERT_RETURN(instance_index < block.scene_instances.size());

	// Unregister the scene instance
	unsigned int instance_count = block.scene_instances.size();
	const unsigned int last_instance_index = --instance_count;
	SceneInstance moved_instance = block.scene_instances[last_instance_index];
	if (instance_index != last_instance_index) {
		// Update last instance index because we did a swap-remove
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

void VoxelInstancer::on_data_block_saved(Vector3i data_grid_position, unsigned int lod_index) {
	if (lod_index >= _lods.size()) {
		return;
	}
	Lod &lod = _lods[lod_index];
	if (lod.quick_reload_cache != nullptr) {
		MutexLock mlock(lod.quick_reload_cache->mutex);
		lod.quick_reload_cache->map.erase(data_grid_position);
	}
}

void VoxelInstancer::set_mesh_block_size_po2(unsigned int p_mesh_block_size_po2) {
	_parent_mesh_block_size_po2 = p_mesh_block_size_po2;
}

void VoxelInstancer::set_data_block_size_po2(unsigned int p_data_block_size_po2) {
	_parent_data_block_size_po2 = p_data_block_size_po2;
}

int VoxelInstancer::get_library_item_id_from_render_block_index(unsigned int render_block_index) const {
	ZN_ASSERT_RETURN_V(render_block_index < _blocks.size(), -1);
	Block &block = *_blocks[render_block_index];
	return block.layer_id;
}

// DEBUG LAND

int VoxelInstancer::debug_get_block_count() const {
	return _blocks.size();
}

void VoxelInstancer::debug_get_instance_counts(StdUnorderedMap<uint32_t, uint32_t> &counts_per_layer) const {
	ZN_PROFILE_SCOPE();

	counts_per_layer.clear();

	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		const Block &block = **it;

		uint32_t count = block.scene_instances.size();

		if (block.multimesh_instance.is_valid()) {
			Ref<MultiMesh> multimesh = block.multimesh_instance.get_multimesh();
			ZN_ASSERT_CONTINUE(multimesh.is_valid());

			count += zylann::godot::get_visible_instance_count(**multimesh);
		}

		counts_per_layer[block.layer_id] += count;
	}
}

Dictionary VoxelInstancer::_b_debug_get_instance_counts() const {
	Dictionary d;
	StdUnorderedMap<uint32_t, uint32_t> map;
	debug_get_instance_counts(map);
	for (auto it = map.begin(); it != map.end(); ++it) {
		d[it->first] = it->second;
	}
	return d;
}

void VoxelInstancer::debug_dump_as_scene(String fpath) const {
	Node *root = debug_dump_as_nodes();
	ERR_FAIL_COND(root == nullptr);

	zylann::godot::set_nodes_owner_except_root(root, root);

	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();
	const Error pack_result = packed_scene->pack(root);
	memdelete(root);
	ERR_FAIL_COND(pack_result != OK);

	const Error save_result = zylann::godot::save_resource(packed_scene, fpath, ResourceSaver::FLAG_BUNDLE_RESOURCES);
	ERR_FAIL_COND(save_result != OK);
}

Node *VoxelInstancer::debug_dump_as_nodes() const {
	const unsigned int mesh_block_size = 1 << _parent_mesh_block_size_po2;

	Node3D *root = memnew(Node3D);
	root->set_transform(get_transform());
	root->set_name("VoxelInstancerRoot");

	StdUnorderedMap<Ref<Mesh>, Ref<Mesh>> mesh_copies;

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

Dictionary VoxelInstancer::debug_get_block_infos(const Vector3 world_position, const int item_id) {
#ifndef TOOLS_ENABLED
	return Dictionary();
#else
	Dictionary dict;

	auto layer_it = _layers.find(item_id);
	if (layer_it == _layers.end()) {
		ZN_PRINT_ERROR("Invalid item id");
		return Dictionary();
	}
	const Layer &layer = layer_it->second;

	dict["lod_index"] = layer.lod_index;

	const int block_shift = _parent_mesh_block_size_po2 + layer.lod_index;
	const Vector3i bpos = Vector3i(world_position.floor()) >> block_shift;
	const Vector3i block_origin_in_voxels = bpos << block_shift;
	const int block_size_in_voxels = 1 << block_shift;

	dict["aabb"] =
			AABB(block_origin_in_voxels, Vector3(block_size_in_voxels, block_size_in_voxels, block_size_in_voxels));

	dict["grid_position"] = bpos;

	auto block_it = layer.blocks.find(bpos);
	const bool allocated = (block_it != layer.blocks.end());
	dict["allocated"] = allocated;
	if (!allocated) {
		return dict;
	}

	const unsigned int block_index = block_it->second;
	const Block *block = _blocks[block_index].get();
	ZN_ASSERT(block != nullptr);

	dict["mesh_lod"] = block->current_mesh_lod;

	Array instances_array;
	Array bodies_array;
	Array scenes_array;

	if (block->multimesh_instance.is_valid()) {
		Ref<MultiMesh> mm = block->multimesh_instance.get_multimesh();

		if (mm.is_valid()) {
			const unsigned int count = zylann::godot::get_visible_instance_count(**mm);
			instances_array.resize(count);

			for (unsigned int instance_index = 0; instance_index < count; ++instance_index) {
				// TODO Optimize: This is very slow the first time, and there is overhead even after that.
				const Transform3D instance_transform = mm->get_instance_transform(instance_index);
				instances_array[instance_index] = instance_transform;
			}
		}
	}

	for (const VoxelInstancerRigidBody *body : block->bodies) {
		bodies_array.push_back(body);
	}

	for (const SceneInstance &scene : block->scene_instances) {
		scenes_array.push_back(scene.root);
	}

	dict["instances"] = instances_array;
	dict["bodies"] = bodies_array;
	dict["scenes"] = scenes_array;

	return dict;
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
				ZN_TTR("This node must be child of a {0}.").format(varray(VoxelLodTerrain::get_class_static()))
		);
	}
	if (_library.is_null()) {
		warnings.append(ZN_TTR("No library is assigned. A {0} is needed to spawn items.")
								.format(varray(VoxelInstanceLibrary::get_class_static())));
	} else if (_library->get_item_count() == 0) {
		warnings.append(ZN_TTR("The assigned library is empty. Add items to it so they can be spawned."));

	} else {
		zylann::godot::get_resource_configuration_warnings(**_library, warnings, []() { return "library: "; });

		VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(_parent);
		if (vt != nullptr) {
			_library->for_each_item([&warnings](int id, const VoxelInstanceLibraryItem &item) {
				const int lod_index = item.get_lod_index();
				if (lod_index > 0) {
					warnings.append(
							String(ZN_TTR("library: item {0}: LOD index is set to higher than 0 ({1}), but the parent "
										  "terrain doesn't have LOD support. Instances will not be generated."))
									.format(varray(id, lod_index))
					);
				}
			});
		}

		if (_parent != nullptr) {
			Ref<VoxelStream> stream = _parent->get_stream();
			if (stream.is_valid() && !stream->supports_instance_blocks()) {
				const int persistent_id = _library->find_item([](const VoxelInstanceLibraryItem &item) { //
					return item.is_persistent();
				});
				if (persistent_id != -1) {
					warnings.append(String(ZN_TTR("Library contains at least one persistent item (ID {0}), but the "
												  "current stream ({1}) does not support saving instances."))
											.format(varray(persistent_id, stream->get_class())));
				}
			}
		}
	}
}

#endif // TOOLS_ENABLED

void VoxelInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_library", "library"), &VoxelInstancer::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &VoxelInstancer::get_library);

	ClassDB::bind_method(D_METHOD("set_up_mode", "mode"), &VoxelInstancer::set_up_mode);
	ClassDB::bind_method(D_METHOD("get_up_mode"), &VoxelInstancer::get_up_mode);

	ClassDB::bind_method(
			D_METHOD("remove_instances_in_sphere", "center", "radius"), &VoxelInstancer::remove_instances_in_sphere
	);

	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelInstancer::debug_get_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_instance_counts"), &VoxelInstancer::_b_debug_get_instance_counts);
	ClassDB::bind_method(D_METHOD("debug_dump_as_scene", "fpath"), &VoxelInstancer::debug_dump_as_scene);
	ClassDB::bind_method(D_METHOD("debug_set_draw_enabled", "enabled"), &VoxelInstancer::debug_set_draw_enabled);
	ClassDB::bind_method(D_METHOD("debug_is_draw_enabled"), &VoxelInstancer::debug_is_draw_enabled);
	ClassDB::bind_method(D_METHOD("debug_set_draw_flag", "flag", "enabled"), &VoxelInstancer::debug_set_draw_flag);
	ClassDB::bind_method(D_METHOD("debug_get_draw_flag", "flag"), &VoxelInstancer::debug_get_draw_flag);
	ClassDB::bind_method(
			D_METHOD("debug_get_block_infos", "world_position", "item_id"), &VoxelInstancer::debug_get_block_infos
	);

	ClassDB::bind_method(
			D_METHOD("get_mesh_lod_update_budget_microseconds"),
			&VoxelInstancer::get_mesh_lod_update_budget_microseconds
	);
	ClassDB::bind_method(
			D_METHOD("set_mesh_lod_update_budget_microseconds", "micros"),
			&VoxelInstancer::set_mesh_lod_update_budget_microseconds
	);

	ClassDB::bind_method(
			D_METHOD("get_collision_update_budget_microseconds"),
			&VoxelInstancer::get_collision_update_budget_microseconds
	);
	ClassDB::bind_method(
			D_METHOD("set_collision_update_budget_microseconds", "micros"),
			&VoxelInstancer::set_collision_update_budget_microseconds
	);

	ClassDB::bind_method(D_METHOD("set_fading_enabled", "enabled"), &VoxelInstancer::set_fading_enabled);
	ClassDB::bind_method(D_METHOD("get_fading_enabled"), &VoxelInstancer::get_fading_enabled);

	ClassDB::bind_method(D_METHOD("set_fading_duration", "duration"), &VoxelInstancer::set_fading_duration);
	ClassDB::bind_method(D_METHOD("get_fading_duration"), &VoxelInstancer::get_fading_duration);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE, VoxelInstanceLibrary::get_class_static()
			),
			"set_library",
			"get_library"
	);
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "up_mode", PROPERTY_HINT_ENUM, "PositiveY,Sphere"), "set_up_mode", "get_up_mode"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "mesh_lod_update_budget_microseconds"),
			"set_mesh_lod_update_budget_microseconds",
			"get_mesh_lod_update_budget_microseconds"
	);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "collision_update_budget_microseconds"),
			"set_collision_update_budget_microseconds",
			"get_collision_update_budget_microseconds"
	);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fading_enabled"), "set_fading_enabled", "get_fading_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fading_duration"), "set_fading_duration", "get_fading_duration");

	BIND_CONSTANT(MAX_LOD);

	BIND_ENUM_CONSTANT(UP_MODE_POSITIVE_Y);
	BIND_ENUM_CONSTANT(UP_MODE_SPHERE);

	BIND_ENUM_CONSTANT(DEBUG_DRAW_ALL_BLOCKS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_EDITED_BLOCKS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_FLAGS_COUNT);
}

} // namespace zylann::voxel
