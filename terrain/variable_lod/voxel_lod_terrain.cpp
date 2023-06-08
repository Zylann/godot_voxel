#include "voxel_lod_terrain.h"
#include "../../constants/voxel_string_names.h"
#include "../../edition/voxel_tool_lod_terrain.h"
#include "../../engine/detail_rendering.h"
#include "../../engine/load_all_blocks_data_task.h"
#include "../../engine/voxel_engine_gd.h"
#include "../../engine/voxel_engine_updater.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/classes/base_material_3d.h" // For property hint in release mode in GDExtension...
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/concave_polygon_shape_3d.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/resource_saver.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/classes/script.h"
#include "../../util/godot/classes/shader.h"
#include "../../util/godot/classes/viewport.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/funcs.h"
#include "../../util/log.h"
#include "../../util/math/color.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/async_dependency_tracker.h"
#include "../../util/thread/mutex.h"
#include "../../util/thread/rw_lock.h"
#include "../instancing/voxel_instancer.h"
#include "voxel_lod_terrain_update_task.h"

namespace zylann::voxel {

namespace {

struct BeforeUnloadMeshAction {
	ShaderMaterialPoolVLT &shader_material_pool;

	void operator()(VoxelMeshBlockVLT &block) {
		ZN_PROFILE_SCOPE_NAMED("Recycle material");
		// Recycle material
		Ref<ShaderMaterial> sm = block.get_shader_material();
		if (sm.is_valid()) {
			shader_material_pool.recycle(sm);
			block.set_shader_material(Ref<ShaderMaterial>());
		}
	}
};

static inline uint64_t get_ticks_msec() {
	return Time::get_singleton()->get_ticks_msec();
}

} // namespace

void ShaderMaterialPoolVLT::recycle(Ref<ShaderMaterial> material) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(material.is_valid());

	// Reset textures to avoid hoarding them in the pool
	material->set_shader_parameter(VoxelStringNames::get_singleton().u_voxel_normalmap_atlas, Ref<Texture2D>());
	material->set_shader_parameter(VoxelStringNames::get_singleton().u_voxel_cell_lookup, Ref<Texture2D>());
	material->set_shader_parameter(
			VoxelStringNames::get_singleton().u_voxel_virtual_texture_offset_scale, Vector4(0, 0, 0, 1));
	// TODO Would be nice if we repurposed `u_transition_mask` to store extra flags.
	// Here we exploit cell_size==0 as "there is no virtual normalmaps on this block"
	material->set_shader_parameter(VoxelStringNames::get_singleton().u_voxel_cell_size, 0.f);
	material->set_shader_parameter(VoxelStringNames::get_singleton().u_voxel_virtual_texture_fade, 0.f);

	material->set_shader_parameter(VoxelStringNames::get_singleton().u_transition_mask, 0);
	material->set_shader_parameter(VoxelStringNames::get_singleton().u_lod_fade, Vector2(0.0, 0.0));

	ShaderMaterialPool::recycle(material);
}

void VoxelLodTerrain::ApplyMeshUpdateTask::run(TimeSpreadTaskContext &ctx) {
	if (!VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
		// The node can have been destroyed while this task was still pending
		ZN_PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
		return;
	}

	std::unordered_map<Vector3i, RefCount> &queued_tasks_in_lod = self->_queued_main_thread_mesh_updates[data.lod];
	auto it = queued_tasks_in_lod.find(data.position);
	if (it != queued_tasks_in_lod.end()) {
		RefCount &count = it->second;
		count.remove();
		if (count.get() > 0) {
			// This is not the only main thread task queued for this block.
			// Cancel it to avoid buildup.
			return;
		}
		queued_tasks_in_lod.erase(it);
	}

	self->apply_mesh_update(data);
}

VoxelLodTerrain::VoxelLodTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	ZN_PRINT_VERBOSE("Construct VoxelLodTerrain");

	_data = make_shared_instance<VoxelData>();
	_update_data = make_shared_instance<VoxelLodTerrainUpdateData>();
	_update_data->task_is_complete = true;
	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_meshing_dependency = make_shared_instance<MeshingDependency>();

	set_notify_transform(true);

	// Doing this to setup the defaults
	set_process_callback(_process_callback);

	// Infinite by default
	_data->set_bounds(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));

	// Mesh updates are spread over frames by scheduling them in a task runner of VoxelEngine,
	// but instead of using a reception buffer we use a callback,
	// because this kind of task scheduling would otherwise delay the update by 1 frame
	VoxelEngine::VolumeCallbacks callbacks;
	callbacks.data = this;
	callbacks.mesh_output_callback = [](void *cb_data, VoxelEngine::BlockMeshOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->get_volume_id();
		task->self = self;
		task->data = std::move(ob);
		VoxelEngine::get_singleton().push_main_thread_time_spread_task(task);

		// If two tasks are queued for the same mesh, cancel the old ones.
		// This is for cases where creating the mesh is slower than the speed at which it is generated,
		// which can cause a buildup that never seems to stop.
		// This is at the expense of holes appearing until all tasks are done.
		std::unordered_map<Vector3i, RefCount> &queued_tasks_in_lod = self->_queued_main_thread_mesh_updates[ob.lod];
		auto p = queued_tasks_in_lod.insert({ ob.position, RefCount(1) });
		if (!p.second) {
			p.first->second.add();
		}
	};
	callbacks.data_output_callback = [](void *cb_data, VoxelEngine::BlockDataOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		self->apply_data_block_response(ob);
	};
	callbacks.virtual_texture_output_callback = [](void *cb_data, VoxelEngine::BlockDetailTextureOutput &ob) {
		VoxelLodTerrain *self = reinterpret_cast<VoxelLodTerrain *>(cb_data);
		self->apply_detail_texture_update(ob);
	};

	_volume_id = VoxelEngine::get_singleton().add_volume(callbacks);
	// VoxelEngine::get_singleton().set_volume_octree_lod_distance(_volume_id, get_lod_distance());

	// TODO Being able to set a LOD smaller than the stream is probably a bad idea,
	// Because it prevents edits from propagating up to the last one, they will be left out of sync
	set_lod_count(4);

	set_lod_distance(48.f);
}

VoxelLodTerrain::~VoxelLodTerrain() {
	ZN_PRINT_VERBOSE("Destroy VoxelLodTerrain");
	abort_async_edits();
	_streaming_dependency->valid = false;
	_meshing_dependency->valid = false;
	VoxelEngine::get_singleton().remove_volume(_volume_id);
	// Instancer can take care of itself
}

Ref<Material> VoxelLodTerrain::get_material() const {
	return _material;
}

void VoxelLodTerrain::set_material(Ref<Material> p_material) {
	if (_material == p_material) {
		return;
	}

	// TODO Update existing block surfaces
	_material = p_material;

	update_shader_material_pool_template();

#ifdef TOOLS_ENABLED
	// Create a fork of the default shader if a new empty ShaderMaterial is assigned
	if (Engine::get_singleton()->is_editor_hint()) {
		Ref<ShaderMaterial> sm = p_material;
		if (sm.is_valid() && sm->get_shader().is_null() && _mesher.is_valid()) {
			Ref<ShaderMaterial> default_sm = _mesher->get_default_lod_material();
			if (default_sm.is_valid()) {
				Ref<Shader> default_shader = default_sm->get_shader();
				ZN_ASSERT_RETURN(default_shader.is_valid());
				Ref<Shader> shader_copy = default_shader->duplicate();
				sm->set_shader(shader_copy);
			}
		}

		update_configuration_warnings();
	}
#endif
}

unsigned int VoxelLodTerrain::get_data_block_size() const {
	return _data->get_block_size();
}

unsigned int VoxelLodTerrain::get_data_block_size_pow2() const {
	return _data->get_block_size_po2();
}

unsigned int VoxelLodTerrain::get_mesh_block_size_pow2() const {
	return _update_data->settings.mesh_block_size_po2;
}

unsigned int VoxelLodTerrain::get_mesh_block_size() const {
	return 1 << _update_data->settings.mesh_block_size_po2;
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream == get_stream()) {
		return;
	}

	_data->set_stream(p_stream);

	StreamingDependency::reset(_streaming_dependency, p_stream, get_generator());

#ifdef TOOLS_ENABLED
	if (p_stream.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> stream_script = p_stream->get_script();
			if (stream_script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_update_data->settings.run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelStream> VoxelLodTerrain::get_stream() const {
	return _data->get_stream();
}

void VoxelLodTerrain::set_generator(Ref<VoxelGenerator> p_generator) {
	if (p_generator == get_generator()) {
		return;
	}

	_data->set_generator(p_generator);

	MeshingDependency::reset(_meshing_dependency, _mesher, p_generator);
	StreamingDependency::reset(_streaming_dependency, get_stream(), p_generator);

#ifdef TOOLS_ENABLED
	if (p_generator.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> generator_script = p_generator->get_script();
			if (generator_script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_update_data->settings.run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelGenerator> VoxelLodTerrain::get_generator() const {
	return _data->get_generator();
}

void VoxelLodTerrain::_on_gi_mode_changed() {
	const GIMode gi_mode = get_gi_mode();
	for (unsigned int lod_index = 0; lod_index < _update_data->state.lods.size(); ++lod_index) {
		_mesh_maps_per_lod[lod_index].for_each_block([gi_mode](VoxelMeshBlockVLT &block) { //
			block.set_gi_mode(DirectMeshInstance::GIMode(gi_mode));
		});
	}
}

void VoxelLodTerrain::_on_shadow_casting_changed() {
	const RenderingServer::ShadowCastingSetting mode = RenderingServer::ShadowCastingSetting(get_shadow_casting());
	for (unsigned int lod_index = 0; lod_index < _update_data->state.lods.size(); ++lod_index) {
		_mesh_maps_per_lod[lod_index].for_each_block([mode](VoxelMeshBlockVLT &block) { //
			block.set_shadow_casting(mode);
		});
	}
}

void VoxelLodTerrain::update_shader_material_pool_template() {
	Ref<ShaderMaterial> shader_material = _material;
	if (_material.is_null() && _mesher.is_valid()) {
		shader_material = _mesher->get_default_lod_material();
	}
	_shader_material_pool.set_template(shader_material);
}

void VoxelLodTerrain::set_mesher(Ref<VoxelMesher> p_mesher) {
	if (_mesher == p_mesher) {
		return;
	}

	stop_updater();

	_mesher = p_mesher;

	update_shader_material_pool_template();

	MeshingDependency::reset(_meshing_dependency, _mesher, get_generator());

	if (_mesher.is_valid()) {
		start_updater();
		remesh_all_blocks();
	}

	update_configuration_warnings();
}

Ref<VoxelMesher> VoxelLodTerrain::get_mesher() const {
	return _mesher;
}

void VoxelLodTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	Ref<VoxelStream> stream = get_stream();

	if (stream.is_valid()) {
		// const int stream_block_size_po2 = _stream->get_block_size_po2();
		//_set_block_size_po2(stream_block_size_po2);

		// TODO We have to figure out streams that have a LOD requirement
		// const int stream_lod_count = _stream->get_lod_count();
		// _set_lod_count(min(stream_lod_count, get_lod_count()));

		if (is_full_load_mode_enabled() && !stream->supports_loading_all_blocks()) {
			ERR_PRINT("The chosen stream does not supports loading all blocks. Full load mode cannot be used.");
			_update_data->wait_for_end_of_task();
			_data->set_streaming_enabled(true);
#ifdef TOOLS_ENABLED
			notify_property_list_changed();
#endif
		}
	}

	reset_maps();
	// TODO Size other than 16 is not really supported though.
	// also this code isn't right, it doesn't update the other lods
	//_data->lods[0].map.create(p_block_size_po2, 0);

	Ref<VoxelGenerator> generator = get_generator();

	if ((stream.is_valid() || generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || _update_data->settings.run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	_update_data->wait_for_end_of_task();
	_update_data->state.force_update_octrees_next_update = true;

	// The whole map might change, so make all area dirty
	const unsigned int lod_count = get_lod_count();
	for (unsigned int i = 0; i < lod_count; ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.last_view_distance_data_blocks = 0;
		lod.last_view_distance_mesh_blocks = 0;
	}

	update_configuration_warnings();
}

void VoxelLodTerrain::set_mesh_block_size(unsigned int mesh_block_size) {
	// Mesh block size cannot be smaller than data block size, for now
	mesh_block_size = math::clamp(mesh_block_size, get_data_block_size(), constants::MAX_BLOCK_SIZE);

	// Only these sizes are allowed at the moment. This stuff is still not supported in a generic way yet,
	// some code still exploits the fact it's a multiple of data block size, for performance
	unsigned int po2;
	switch (mesh_block_size) {
		case 16:
			po2 = 4;
			break;
		case 32:
			po2 = 5;
			break;
		default:
			mesh_block_size = 16;
			po2 = 4;
			break;
	}
	if (mesh_block_size == get_mesh_block_size()) {
		return;
	}

	reset_mesh_maps();

	//_update_data->wait_for_end_of_task(); // Done by reset_mesh_maps()
	ZN_ASSERT(_update_data->task_is_complete);
	_update_data->settings.mesh_block_size_po2 = po2;
	_update_data->state.force_update_octrees_next_update = true;

	// Doing this after because `on_mesh_block_exit` may use the old size
	if (_instancer != nullptr) {
		_instancer->set_mesh_block_size_po2(mesh_block_size);
	}

	// Update voxel bounds because block size change can affect octree size
	set_voxel_bounds(_data->get_bounds());
}

void VoxelLodTerrain::set_full_load_mode_enabled(bool enabled) {
	const bool streaming_enabled = !enabled;
	if (streaming_enabled != _data->is_streaming_enabled()) {
		_update_data->wait_for_end_of_task();
		//_update_data->settings.full_load_mode = enabled;
		_data->set_streaming_enabled(streaming_enabled);
		_update_data->state.force_update_octrees_next_update = true;
		_on_stream_params_changed();
	}
}

bool VoxelLodTerrain::is_full_load_mode_enabled() const {
	return !_data->is_streaming_enabled();
}

void VoxelLodTerrain::set_threaded_update_enabled(bool enabled) {
	if (enabled != _threaded_update_enabled) {
		if (_threaded_update_enabled) {
			_update_data->wait_for_end_of_task();
		}
		_threaded_update_enabled = enabled;
	}
}

bool VoxelLodTerrain::is_threaded_update_enabled() const {
	return _threaded_update_enabled;
}

void VoxelLodTerrain::set_mesh_block_active(VoxelMeshBlockVLT &block, bool active, bool with_fading) {
	if (block.active == active) {
		return;
	}

	// TODO Shouldn't we switch colliders with `active` instead of `visible`?
	block.active = active;

	if (!with_fading) {
		block.set_visible(active);

		// Cancel fading if already in progress
		if (block.fading_state != VoxelMeshBlockVLT::FADING_NONE) {
			block.fading_state = VoxelMeshBlockVLT::FADING_NONE;

			_fading_blocks_per_lod[block.lod_index].erase(block.position);

			Ref<ShaderMaterial> mat = block.get_shader_material();
			if (mat.is_valid()) {
				mat->set_shader_parameter(VoxelStringNames::get_singleton().u_lod_fade, Vector2(0.0, 0.0));
			}

		} else if (active && _lod_fade_duration > 0.f) {
			// WHen LOD fade is enabled, it is possible that a block is disabled with a fade out, but later has to be
			// enabled without a fade-in (because behind the camera for example). In this case we have to reset the
			// parameter. Otherwise, it would be active but invisible due to still being faded out.
			Ref<ShaderMaterial> mat = block.get_shader_material();
			if (mat.is_valid()) {
				mat->set_shader_parameter(VoxelStringNames::get_singleton().u_lod_fade, Vector2(0.0, 0.0));
			}
		}

		return;
	}

	VoxelMeshBlockVLT::FadingState fading_state;
	// Initial progress has to be set too because it sometimes happens that a LOD must appear before its parent
	// finished fading in. So the parent will have to fade out from solid with the same duration.
	float initial_progress;
	if (active) {
		block.set_visible(true);
		fading_state = VoxelMeshBlockVLT::FADING_IN;
		initial_progress = 0.f;
	} else {
		fading_state = VoxelMeshBlockVLT::FADING_OUT;
		initial_progress = 1.f;
	}

	if (block.fading_state != fading_state) {
		if (block.fading_state == VoxelMeshBlockVLT::FADING_NONE) {
			std::map<Vector3i, VoxelMeshBlockVLT *> &fading_blocks = _fading_blocks_per_lod[block.lod_index];
			// Must not have duplicates
			ERR_FAIL_COND(fading_blocks.find(block.position) != fading_blocks.end());
			fading_blocks.insert({ block.position, &block });
		}
		block.fading_state = fading_state;
		block.fading_progress = initial_progress;
	}
}

// Marks intersecting blocks in the area as modified, updates LODs and schedules remeshing.
// The provided box must be at LOD0 coordinates.
void VoxelLodTerrain::post_edit_area(Box3i p_box) {
	ZN_PROFILE_SCOPE();
	// TODO Better decoupling is needed here.
	// In the past this padding was necessary for mesh blocks because visuals depend on neighbor voxels.
	// So when editing voxels at the boundary of two mesh blocks, both must update.
	// However on data blocks it doesn't make sense, neighbors are not affected (at least for now).
	// this can cause false positive errors as if we were editing a block that's not loaded (coming up as null).
	// For now, this is worked around by ignoring cases where blocks are null,
	// But it might mip more lods than necessary when editing on borders.
	const Box3i box = p_box.padded(1);
	{
		MutexLock lock(_update_data->state.blocks_pending_lodding_lod0_mutex);
		_data->mark_area_modified(box, &_update_data->state.blocks_pending_lodding_lod0);
	}

#ifdef TOOLS_ENABLED
	if (debug_is_draw_enabled() && debug_get_draw_flag(DEBUG_DRAW_EDIT_BOXES)) {
		_debug_edit_items.push_back({ p_box, DebugEditItem::LINGER_FRAMES });
	}
#endif

	if (_instancer != nullptr) {
		_instancer->on_area_edited(p_box);
	}
}

void VoxelLodTerrain::post_edit_modifiers(Box3i p_voxel_box) {
	// clear_cached_blocks_in_voxel_area(*_data, p_voxel_box);
	_data->clear_cached_blocks_in_voxel_area(p_voxel_box);
	// Not sure if it is worth re-caching these blocks. We may see about that in the future if performance is an issue.

	MutexLock lock(_update_data->state.changed_generated_areas_mutex);
	_update_data->state.changed_generated_areas.push_back(p_voxel_box);

#ifdef TOOLS_ENABLED
	if (debug_is_draw_enabled() && debug_get_draw_flag(DEBUG_DRAW_EDIT_BOXES)) {
		_debug_edit_items.push_back({ p_voxel_box, DebugEditItem::LINGER_FRAMES });
	}
#endif
}

void VoxelLodTerrain::push_async_edit(IThreadedTask *task, Box3i box, std::shared_ptr<AsyncDependencyTracker> tracker) {
	CRASH_COND(task == nullptr);
	CRASH_COND(tracker == nullptr);

	VoxelLodTerrainUpdateData::AsyncEdit e;
	e.box = box;
	e.task = task;
	e.task_tracker = tracker;

	VoxelLodTerrainUpdateData::State &state = _update_data->state;
	MutexLock lock(state.pending_async_edits_mutex);
	state.pending_async_edits.push_back(e);
}

Ref<VoxelTool> VoxelLodTerrain::get_voxel_tool() {
	VoxelToolLodTerrain *vt = memnew(VoxelToolLodTerrain(this));
	// Set to most commonly used channel on this kind of terrain
	vt->set_channel(VoxelBufferInternal::CHANNEL_SDF);
	return Ref<VoxelTool>(vt);
}

int VoxelLodTerrain::get_view_distance() const {
	return _update_data->settings.view_distance_voxels;
}

// TODO Needs to be clamped dynamically, to avoid the user accidentally setting blowing up memory.
// It used to be clamped to a hardcoded value, but now it may depend on LOD count and boundaries
void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {
	ERR_FAIL_COND(p_distance_in_voxels <= 0);
	// Note: this is a hint distance, the terrain will attempt to have this radius filled with loaded voxels.
	// It is possible for blocks to still load beyond that distance.
	_update_data->wait_for_end_of_task();
	_update_data->settings.view_distance_voxels = p_distance_in_voxels;
	_update_data->state.force_update_octrees_next_update = true;
}

void VoxelLodTerrain::start_updater() {
	Ref<VoxelMesherBlocky> blocky_mesher = _mesher;
	if (blocky_mesher.is_valid()) {
		Ref<VoxelBlockyLibrary> library = blocky_mesher->get_library();
		if (library.is_valid()) {
			// TODO Any way to execute this function just after the TRES resource loader has finished to load?
			// VoxelLibrary should be baked ahead of time, like MeshLibrary
			library->bake();
		}
	}
}

void VoxelLodTerrain::stop_updater() {
	// Invalidate pending tasks
	MeshingDependency::reset(_meshing_dependency, _mesher, get_generator());
	// VoxelEngine::get_singleton().set_volume_mesher(_volume_id, Ref<VoxelMesher>());

	// TODO We can still receive a few mesh delayed mesh updates after this. Is it a problem?
	//_reception_buffers.mesh_output.clear();

	_update_data->wait_for_end_of_task();

	for (unsigned int i = 0; i < _update_data->state.lods.size(); ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.blocks_pending_update.clear();

		for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = it->second;
			if (mesh_block.state == VoxelLodTerrainUpdateData::MESH_UPDATE_SENT) {
				mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
			}
		}
	}
}

void VoxelLodTerrain::start_streamer() {
	if (is_full_load_mode_enabled() && get_stream().is_valid()) {
		// TODO May want to defer this to be sure it's not done multiple times.
		// This would be a side-effect of setting properties one by one, either by scene loader or by script

		ZN_PRINT_VERBOSE(format("Request all blocks for volume {}", _volume_id));
		ZN_ASSERT(_streaming_dependency != nullptr);

		LoadAllBlocksDataTask *task = memnew(LoadAllBlocksDataTask);
		task->volume_id = _volume_id;
		task->stream_dependency = _streaming_dependency;

		VoxelEngine::get_singleton().push_async_io_task(task);
	}
}

void VoxelLodTerrain::stop_streamer() {
	_update_data->wait_for_end_of_task();

	for (unsigned int i = 0; i < _update_data->state.lods.size(); ++i) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[i];
		lod.loading_blocks.clear();
	}

	//_reception_buffers.data_output.clear();
}

void VoxelLodTerrain::set_lod_distance(float p_lod_distance) {
	if (p_lod_distance == _update_data->settings.lod_distance) {
		return;
	}

	_update_data->wait_for_end_of_task();

	// Distance must be greater than a threshold,
	// otherwise lods will decimate too fast and it will look messy
	const float lod_distance =
			math::clamp(p_lod_distance, constants::MINIMUM_LOD_DISTANCE, constants::MAXIMUM_LOD_DISTANCE);
	_update_data->settings.lod_distance = lod_distance;
	_update_data->state.force_update_octrees_next_update = true;
	// VoxelEngine::get_singleton().set_volume_octree_lod_distance(_volume_id, get_lod_distance());

	if (_instancer != nullptr) {
		_instancer->set_mesh_lod_distance(lod_distance);
	}
}

float VoxelLodTerrain::get_lod_distance() const {
	return _update_data->settings.lod_distance;
}

void VoxelLodTerrain::set_lod_count(int p_lod_count) {
	ERR_FAIL_COND(p_lod_count >= (int)constants::MAX_LOD);
	ERR_FAIL_COND(p_lod_count < 1);

	if (get_lod_count() != p_lod_count) {
		_set_lod_count(p_lod_count);
	}
}

void VoxelLodTerrain::_set_lod_count(int p_lod_count) {
	CRASH_COND(p_lod_count >= (int)constants::MAX_LOD);
	CRASH_COND(p_lod_count < 1);

	_update_data->wait_for_end_of_task();

	_data->set_lod_count(p_lod_count);
	_update_data->state.force_update_octrees_next_update = true;

	LodOctree::NoDestroyAction nda;

	std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem> &octrees = _update_data->state.lod_octrees;
	for (auto it = octrees.begin(); it != octrees.end(); ++it) {
		VoxelLodTerrainUpdateData::OctreeItem &item = it->second;
		item.octree.create(p_lod_count, nda);
	}

	for (unsigned int i = 0; i < _queued_main_thread_mesh_updates.size(); ++i) {
		_queued_main_thread_mesh_updates[i].clear();
	}

	// Not entirely required, but changing LOD count at runtime is rarely needed
	reset_maps();
}

void VoxelLodTerrain::reset_maps() {
	// Clears all blocks and reconfigures maps to account for new LOD count and block sizes

	// Don't reset while streaming, the result can be dirty?
	// CRASH_COND(_stream_thread != nullptr);

	_update_data->wait_for_end_of_task();

	_data->reset_maps();

	abort_async_edits();

	reset_mesh_maps();
}

void VoxelLodTerrain::reset_mesh_maps() {
	_update_data->wait_for_end_of_task();

	const unsigned int lod_count = get_lod_count();
	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

		if (_instancer != nullptr) {
			// Unload instances
			VoxelInstancer *instancer = _instancer;
			mesh_map.for_each_block([lod_index, instancer](VoxelMeshBlockVLT &block) {
				instancer->on_mesh_block_exit(block.position, lod_index);
			});
		}

		// mesh_map.for_each_block(BeforeUnloadMeshAction{ _shader_material_pool });

		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < lod_count) {
			mesh_map.clear();
			// Reset view distance cache so blocks will be re-entered due to the difference
			lod.last_view_distance_data_blocks = 0;
			lod.last_view_distance_mesh_blocks = 0;
		} else {
			mesh_map.clear();
		}

		lod.mesh_map_state.map.clear();

		// Clear temporal lists
		lod.mesh_blocks_to_activate.clear();
		lod.mesh_blocks_to_deactivate.clear();
		lod.mesh_blocks_to_unload.clear();
		lod.mesh_blocks_to_update_transitions.clear();

		_deferred_collision_updates_per_lod[lod_index].clear();
	}

	// Reset LOD octrees
	LodOctree::NoDestroyAction nda;
	for (std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::iterator it = state.lod_octrees.begin();
			it != state.lod_octrees.end(); ++it) {
		VoxelLodTerrainUpdateData::OctreeItem &item = it->second;
		item.octree.create(lod_count, nda);
	}

	// Reset previous state caches to force rebuilding the view area
	state.last_octree_region_box = Box3i();
	state.lod_octrees.clear();
}

int VoxelLodTerrain::get_lod_count() const {
	return _data->get_lod_count();
}

void VoxelLodTerrain::set_generate_collisions(bool enabled) {
	_update_data->settings.collision_enabled = enabled;
}

bool VoxelLodTerrain::get_generate_collisions() const {
	return _update_data->settings.collision_enabled;
}

void VoxelLodTerrain::set_collision_lod_count(int lod_count) {
	ERR_FAIL_COND(lod_count < 0);
	_collision_lod_count = static_cast<unsigned int>(math::min(lod_count, get_lod_count()));
}

int VoxelLodTerrain::get_collision_lod_count() const {
	return _collision_lod_count;
}

void VoxelLodTerrain::set_collision_layer(int layer) {
	const unsigned int lod_count = get_lod_count();

	_collision_layer = layer;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([layer](VoxelMeshBlockVLT &block) { //
			block.set_collision_layer(layer);
		});
	}
}

int VoxelLodTerrain::get_collision_layer() const {
	return _collision_layer;
}

void VoxelLodTerrain::set_collision_mask(int mask) {
	const unsigned int lod_count = get_lod_count();

	_collision_mask = mask;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([mask](VoxelMeshBlockVLT &block) { //
			block.set_collision_mask(mask);
		});
	}
}

int VoxelLodTerrain::get_collision_mask() const {
	return _collision_mask;
}

void VoxelLodTerrain::set_collision_margin(float margin) {
	const unsigned int lod_count = get_lod_count();

	_collision_margin = margin;
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		mesh_map.for_each_block([margin](VoxelMeshBlockVLT &block) { //
			block.set_collision_margin(margin);
		});
	}
}

float VoxelLodTerrain::get_collision_margin() const {
	return _collision_margin;
}

int VoxelLodTerrain::get_data_block_region_extent() const {
	return VoxelEngine::get_octree_lod_block_region_extent(_update_data->settings.lod_distance, get_data_block_size());
}

int VoxelLodTerrain::get_mesh_block_region_extent() const {
	return VoxelEngine::get_octree_lod_block_region_extent(_update_data->settings.lod_distance, get_mesh_block_size());
}

Vector3i VoxelLodTerrain::voxel_to_data_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3i());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3i());
	const Vector3i bpos = _data->voxel_to_block(math::floor_to_int(vpos)) >> lod_index;
	return bpos;
}

Vector3i VoxelLodTerrain::voxel_to_mesh_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3i());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3i());
	const unsigned int mesh_block_size_po2 = _update_data->settings.mesh_block_size_po2;
	const Vector3i bpos = (math::floor_to_int(vpos) >> mesh_block_size_po2) >> lod_index;
	return bpos;
}

void VoxelLodTerrain::set_process_callback(ProcessCallback mode) {
	_process_callback = mode;
	set_process(_process_callback == PROCESS_CALLBACK_IDLE);
	set_physics_process(_process_callback == PROCESS_CALLBACK_PHYSICS);
}

void VoxelLodTerrain::_notification(int p_what) {
	switch (p_what) {
		// TODO Should use NOTIFICATION_INTERNAL_PROCESS instead?
		case NOTIFICATION_PROCESS:
			if (_process_callback == PROCESS_CALLBACK_IDLE) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelEngineUpdater::ensure_existence(get_tree());
				process(get_process_delta_time());
			}
			break;

		// TODO Should use NOTIFICATION_INTERNAL_PHYSICS_PROCESS instead?
		case NOTIFICATION_PHYSICS_PROCESS:
			if (_process_callback == PROCESS_CALLBACK_PHYSICS) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelEngineUpdater::ensure_existence(get_tree());
				process(get_physics_process_delta_time());
			}
			break;

#ifdef TOOLS_ENABLED
		case NOTIFICATION_ENTER_TREE:
			// In the editor, auto-configure a default mesher, for convenience.
			// Because Godot has a property hint to automatically instantiate a resource, but if that resource is
			// abstract, it doesn't work... and it cannot be a default value because such practice was deprecated with a
			// warning in Godot 4.
			if (Engine::get_singleton()->is_editor_hint() && !get_mesher().is_valid()) {
				Ref<VoxelMesherTransvoxel> mesher;
				mesher.instantiate();
				set_mesher(mesher);
			}
			break;
#endif

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			World3D *world = *get_world_3d();
			VoxelLodTerrainUpdateData::State &state = _update_data->state;
			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([world](VoxelMeshBlockVLT &block) { //
					block.set_world(world);
				});
			}
#ifdef TOOLS_ENABLED
			if (debug_is_draw_enabled()) {
				_debug_renderer.set_world(is_visible_in_tree() ? world : nullptr);
			}
#endif
			// DEBUG
			// set_show_gizmos(true);
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			VoxelLodTerrainUpdateData::State &state = _update_data->state;
			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([](VoxelMeshBlockVLT &block) { //
					block.set_world(nullptr);
				});
			}
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(nullptr);
#endif
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			const bool visible = is_visible();
			VoxelLodTerrainUpdateData::State &state = _update_data->state;

			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([visible](VoxelMeshBlockVLT &block) { //
					block.set_parent_visible(visible);
				});
			}

#ifdef TOOLS_ENABLED
			if (debug_is_draw_enabled()) {
				_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
			}
#endif
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			ZN_PROFILE_SCOPE_NAMED("VoxelLodTerrain::NOTIFICATION_TRANSFORM_CHANGED");

			const Transform3D transform = get_global_transform();
			// VoxelEngine::get_singleton().set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			VoxelLodTerrainUpdateData::State &state = _update_data->state;

			for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
				VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
				mesh_map.for_each_block([&transform](VoxelMeshBlockVLT &block) { //
					block.set_parent_transform(transform);
				});
			}

			for (FadingOutMesh &item : _fading_out_meshes) {
				item.mesh_instance.set_transform(transform * Transform3D(Basis(), item.local_position));
			}
		} break;

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_local_viewer_pos() const {
	// Pick this by default
	Vector3 pos = _update_data->state.lods[0].last_viewer_data_block_pos << get_data_block_size_pow2();

	// TODO Support for multiple viewers, this is a placeholder implementation
	VoxelEngine::get_singleton().for_each_viewer( //
			[&pos](ViewerID id, const VoxelEngine::Viewer &viewer) { //
				pos = viewer.world_position;
			});

	const Transform3D world_to_local = get_global_transform().affine_inverse();
	pos = world_to_local.xform(pos);
	return pos;
}

inline bool check_block_sizes(int data_block_size, int mesh_block_size) {
	return (data_block_size == 16 || data_block_size == 32) && (mesh_block_size == 16 || mesh_block_size == 32) &&
			mesh_block_size >= data_block_size;
}

void VoxelLodTerrain::process(float delta) {
	ZN_PROFILE_SCOPE();

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	// TODO It is currently not possible to fully compile those shaders on the fly in a thread.
	// The GLSL functions of VoxelGenerator need to be thread-safe. Compiling should be safe, but getting the source
	// code isn't. VoxelGeneratorGraph's shader generation is not thread-safe, because it accesses its graph.
	if (get_normalmap_use_gpu()) {
		Ref<VoxelGenerator> generator;
		Ref<VoxelGenerator> generator_override = get_normalmap_generator_override();
		if (generator_override.is_valid()) {
			generator = generator_override;
		} else {
			generator = get_generator();
		}
		if (generator.is_valid() && generator->supports_shaders() &&
				generator->get_virtual_rendering_shader() == nullptr) {
			generator->compile_shaders();
		}
	}

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	// process_block_loading_responses();

	// TODO This could go into time spread tasks too
	process_deferred_collision_updates(VoxelEngine::get_singleton().get_main_thread_time_budget_usec());

#ifdef TOOLS_ENABLED
	if (debug_is_draw_enabled() && is_visible_in_tree()) {
		update_gizmos();
	}
#endif

	if (_update_data->task_is_complete) {
		ZN_PROFILE_SCOPE();

		apply_main_thread_update_tasks();

		// Get viewer location in voxel space
		const Vector3 viewer_pos = get_local_viewer_pos();

		// TODO Optimization: pool tasks instead of allocating?
		VoxelLodTerrainUpdateTask *task = memnew(VoxelLodTerrainUpdateTask(_data, _update_data, _streaming_dependency,
				_meshing_dependency, VoxelEngine::get_singleton().get_shared_viewers_data_from_default_world(),
				viewer_pos, _instancer != nullptr, _volume_id, get_global_transform()));

		_update_data->task_is_complete = false;

		if (_threaded_update_enabled) {
			// Schedule task at the end, so it is less likely to have contention with other logic than if it was done at
			// the beginnning of `_process`
			VoxelEngine::get_singleton().push_async_task(task);

		} else {
			task->run(ThreadedTaskContext{ 0 });
			memdelete(task);
			apply_main_thread_update_tasks();
		}
	}

	// Do it after we change mesh block states so materials are updated
	process_fading_blocks(delta);
}

void VoxelLodTerrain::apply_main_thread_update_tasks() {
	ZN_PROFILE_SCOPE();
	// Dequeue outputs of the threadable part of the update for actions taking place on the main thread

	CRASH_COND(_update_data->task_is_complete == false);

	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	// Transitions and fading are visual things, in multiplayer servers they won't be used, so we can take a shortcut
	// and use the camera for them.
	const LocalCameraInfo camera = get_local_camera_info();
	const Transform3D volume_transform = get_global_transform();
	const unsigned int lod_count = get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		std::unordered_set<const VoxelMeshBlockVLT *> activated_blocks;

		const int mesh_block_size = get_mesh_block_size() << lod_index;

		for (unsigned int i = 0; i < lod.mesh_blocks_to_activate.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_activate[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				continue;
			}
			// ERR_CONTINUE(block == nullptr);
			bool with_fading = false;
			if (_lod_fade_duration > 0.f) {
				const Vector3 block_center = volume_transform.xform(
						to_vec3(block->position * mesh_block_size + Vector3iUtil::create(mesh_block_size / 2)));
				// Don't start fading on blocks behind the camera
				with_fading = camera.forward.dot(block_center - camera.position) > 0.0;
			}
			set_mesh_block_active(*block, true, with_fading);
			activated_blocks.insert(block);
		}

		for (unsigned int i = 0; i < lod.mesh_blocks_to_deactivate.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_deactivate[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				continue;
			}
			// ERR_CONTINUE(block == nullptr);
			bool with_fading = false;
			if (_lod_fade_duration > 0.f) {
				const Vector3 block_center = volume_transform.xform(
						to_vec3(block->position * mesh_block_size + Vector3iUtil::create(mesh_block_size / 2)));
				// Don't start fading on blocks behind the camera
				with_fading = camera.forward.dot(block_center - camera.position) > 0.0;
			}
			set_mesh_block_active(*block, false, with_fading);
		}

		lod.mesh_blocks_to_activate.clear();
		lod.mesh_blocks_to_deactivate.clear();

		/*
#ifdef DEBUG_ENABLED
		std::unordered_set<Vector3i> debug_removed_blocks;
#endif
		*/

		for (unsigned int i = 0; i < lod.mesh_blocks_to_unload.size(); ++i) {
			const Vector3i bpos = lod.mesh_blocks_to_unload[i];
			mesh_map.remove_block(bpos, BeforeUnloadMeshAction{ _shader_material_pool });

			_fading_blocks_per_lod[lod_index].erase(bpos);

			if (_instancer != nullptr) {
				_instancer->on_mesh_block_exit(bpos, lod_index);
			}
			/*
#ifdef DEBUG_ENABLED
			debug_removed_blocks.insert(bpos);
#endif
			*/
			// Blocks in the update queue will be cancelled in _process,
			// because it's too expensive to linear-search all blocks for each block
		}

		for (unsigned int i = 0; i < lod.mesh_blocks_to_update_transitions.size(); ++i) {
			const VoxelLodTerrainUpdateData::TransitionUpdate tu = lod.mesh_blocks_to_update_transitions[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(tu.block_position);
			// Can be null if there is actually no surface at this location
			if (block == nullptr) {
				/*
#ifdef DEBUG_ENABLED
				// If the block was removed for a different reason then it is unexpected
				ERR_CONTINUE(debug_removed_blocks.find(tu.block_position) == debug_removed_blocks.end());
#endif
				ZN_PRINT_VERBOSE(String("Skipping TransitionUpdate at {0} lod {1}, block not found")
									  .format(varray(tu.block_position, lod_index)));
				*/
				continue;
			}
			// CRASH_COND(block == nullptr);
			if (block->active) {
				Ref<ShaderMaterial> shader_material = block->get_shader_material();

				// Fade stitching transitions to avoid cracks.
				// This is done by triggering a fade-in on the block, while a copy of it fades out with the previous
				// material settings. This causes a bit of overdraw, but LOD fading does anyways.
				if (_lod_fade_duration > 0.f && shader_material.is_valid() &&
						activated_blocks.find(block) == activated_blocks.end() &&
						tu.transition_mask != block->get_transition_mask()) {
					//
					const Vector3 block_center = volume_transform.xform(
							to_vec3(block->position * mesh_block_size + Vector3iUtil::create(mesh_block_size / 2)));

					// Don't do fading for blocks behind the camera.
					if (camera.forward.dot(block_center - camera.position) > 0.f) {
						FadingOutMesh item;

						item.local_position = block->position * mesh_block_size;
						item.progress = 1.f;

						// Wayyyy too slow, because of https://github.com/godotengine/godot/issues/34741
						// item.shader_material = shader_material->duplicate(false);
						item.shader_material = _shader_material_pool.allocate();
						ZN_ASSERT(item.shader_material.is_valid());
						copy_shader_params(**shader_material, **item.shader_material,
								_shader_material_pool.get_cached_shader_uniforms());

						// item.shader_material->set_shader_param(
						// 		VoxelStringNames::get_singleton().u_lod_fade, Vector2(item.progress, 0.f));

						item.mesh_instance.create();
						item.mesh_instance.set_mesh(block->get_mesh());
						item.mesh_instance.set_gi_mode(DirectMeshInstance::GIMode(get_gi_mode()));
						item.mesh_instance.set_transform(volume_transform * Transform3D(Basis(), item.local_position));
						item.mesh_instance.set_material_override(item.shader_material);
						item.mesh_instance.set_world(*get_world_3d());
						item.mesh_instance.set_visible(true);

						_fading_out_meshes.push_back(std::move(item));

						if (block->fading_state == VoxelMeshBlockVLT::FADING_NONE) {
							_fading_blocks_per_lod[lod_index].insert({ block->position, block });
							block->fading_state = VoxelMeshBlockVLT::FADING_IN;
							block->fading_progress = 0.f;
						}
					}
				}

				block->set_transition_mask(tu.transition_mask);
			}
		}

		lod.mesh_blocks_to_unload.clear();
		lod.mesh_blocks_to_update_transitions.clear();
	}

	// Remove completed async edits
	unordered_remove_if(state.running_async_edits, [this](VoxelLodTerrainUpdateData::RunningAsyncEdit &e) {
		if (e.tracker->is_complete()) {
			if (e.tracker->has_next_tasks()) {
				ERR_PRINT("Completed async edit had next tasks?");
			}
			post_edit_area(e.box);
			return true;

		} else if (e.tracker->is_aborted()) {
			return true;
		}

		return false;
	});

	_stats.blocked_lods = state.stats.blocked_lods;
	_stats.time_detect_required_blocks = state.stats.time_detect_required_blocks;
	_stats.time_io_requests = state.stats.time_io_requests;
	_stats.time_mesh_requests = state.stats.time_mesh_requests;
	_stats.time_update_task = state.stats.time_total;
}

template <typename T>
bool thread_safe_contains(const std::unordered_set<T> &set, T v, BinaryMutex &mutex) {
	MutexLock lock(mutex);
	typename std::unordered_set<T>::const_iterator it = set.find(v);
	return it != set.end();
}

void VoxelLodTerrain::apply_data_block_response(VoxelEngine::BlockDataOutput &ob) {
	ZN_PROFILE_SCOPE();

	if (ob.type == VoxelEngine::BlockDataOutput::TYPE_SAVED) {
		// That's a save confirmation event.
		// Note: in the future, if blocks don't get copied before being sent for saving,
		// we will need to use block versioning to know when we can reset the `modified` flag properly

		// TODO Now that's the case. Use version? Or just keep copying?
		return;
	}

	if (ob.lod >= get_lod_count()) {
		// That block was requested at a time where LOD was higher... drop it
		++_stats.dropped_block_loads;
		return;
	}

	// Initial load will be true when we requested data without specifying specific positions,
	// so we wouldn't know which ones to expect. This is the case of full load mode.
	VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[ob.lod];
	if (!ob.initial_load) {
		if (!thread_safe_contains(lod.loading_blocks, ob.position, lod.loading_blocks_mutex)) {
			// That block was not requested, or is no longer needed. drop it...
			ZN_PRINT_VERBOSE(format("Ignoring block {} lod {}, it was not in loading blocks", ob.position, ob.lod));
			++_stats.dropped_block_loads;
			return;
		}
	}

	if (ob.dropped) {
		// That block was dropped by the data loader thread, but we were still expecting it...
		// This is most likely caused by the loader not keeping up with the speed at which the player is moving.
		// We should recover with the removal from `loading_blocks` so it will be re-queried again later...

		//				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1},
		//{2}, {3})") 								   .format(varray(ob.lod, ob.position.x, ob.position.y,
		// ob.position.z)));

		++_stats.dropped_block_loads;
		return;
	}

	VoxelDataBlock block(ob.voxels, ob.lod);
	block.set_edited(ob.type == VoxelEngine::BlockDataOutput::TYPE_LOADED);

	if (block.has_voxels() && block.get_voxels_const().get_size() != Vector3iUtil::create(_data->get_block_size())) {
		// Voxel block size is incorrect, drop it
		ZN_PRINT_ERROR("Block is different from expected size");
		++_stats.dropped_block_loads;
		return;
	}

	const bool inserted = _data->try_set_block(ob.position, block);
	// TODO Might not ignore these blocks in the future, see `VoxelTerrain`
	if (!inserted) {
		++_stats.dropped_block_loads;
		return;
	}

	{
		// We have to do this after adding the block to the map, otherwise there would be a small period of time where
		// the threaded update task could request the block again needlessly
		MutexLock lock(lod.loading_blocks_mutex);
		lod.loading_blocks.erase(ob.position);
	}

	if (_instancer != nullptr && ob.instances != nullptr) {
		_instancer->on_data_block_loaded(ob.position, ob.lod, std::move(ob.instances));
	}
}

void VoxelLodTerrain::apply_mesh_update(VoxelEngine::BlockMeshOutput &ob) {
	// The following is done on the main thread because Godot doesn't really support everything done here.
	// Building meshes can be done in the threaded task when using Vulkan, but not OpenGL.
	// Setting up mesh instances might not be well threaded?
	// Building collision shapes in threads efficiently is not supported.
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND(!is_inside_tree());

	CRASH_COND(_update_data == nullptr);
	VoxelLodTerrainUpdateData &update_data = *_update_data;

	if (ob.lod >= get_lod_count()) {
		// Sorry, LOD configuration changed, drop that mesh
		++_stats.dropped_block_meshs;
		return;
	}
	// There is a slim chance for some updates to come up just after setting the mesher to null. Avoids a crash.
	if (_mesher.is_null()) {
		++_stats.dropped_block_meshs;
		return;
	}

	uint8_t transition_mask;
	bool active;
	{
		VoxelLodTerrainUpdateData::Lod &lod = update_data.state.lods[ob.lod];
		RWLockRead rlock(lod.mesh_map_state.map_lock);
		auto mesh_block_state_it = lod.mesh_map_state.map.find(ob.position);
		if (mesh_block_state_it == lod.mesh_map_state.map.end()) {
			// That block is no longer loaded in the update map, drop the result
			++_stats.dropped_block_meshs;
			return;
		}
		if (ob.type == VoxelEngine::BlockMeshOutput::TYPE_DROPPED) {
			// That block is loaded, but its meshing request was dropped.
			// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
			ZN_PRINT_VERBOSE("Received a block mesh drop while we were still expecting it");
			++_stats.dropped_block_meshs;
			return;
		}

		VoxelLodTerrainUpdateData::MeshBlockState &mesh_block_state = mesh_block_state_it->second;

		transition_mask = mesh_block_state.transition_mask;

		// The update task could be running at the same time, so we need to do this atomically.
		// The state can become "up to date" only if no other unsent update was pending.
		VoxelLodTerrainUpdateData::MeshState expected = VoxelLodTerrainUpdateData::MESH_UPDATE_SENT;
		mesh_block_state.state.compare_exchange_strong(expected, VoxelLodTerrainUpdateData::MESH_UP_TO_DATE);
		active = mesh_block_state.active;
	}

	// -------- Part where we invoke Godot functions ---------
	// This part is not fully threadable.

	VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[ob.lod];
	VoxelMeshBlockVLT *block = mesh_map.get_block(ob.position);

	VoxelMesher::Output &mesh_data = ob.surfaces;

	Ref<ArrayMesh> mesh;
	if (ob.has_mesh_resource) {
		// The mesh was already built as part of the threaded task
		mesh = ob.mesh;
		// It can be empty
		if (mesh.is_valid()) {
			const unsigned int surface_count = mesh->get_surface_count();
			for (unsigned int surface_index = 0; surface_index < surface_count; ++surface_index) {
				mesh->surface_set_material(surface_index, _material);
			}
		}
	} else {
		// Can't build meshes in threads, do it here
		build_mesh(to_span_const(mesh_data.surfaces), mesh_data.primitive_type, mesh_data.mesh_flags, _material);
	}

	if (mesh.is_null()) {
		// The mesh is empty
		if (block != nullptr) {
			// No surface anymore in this block, destroy it
			// TODO Factor removal in a function, it's done in a few places
			mesh_map.remove_block(ob.position, BeforeUnloadMeshAction{ _shader_material_pool });
			if (_instancer != nullptr) {
				_instancer->on_mesh_block_exit(ob.position, ob.lod);
			}
		}
		// ZN_PRINT_VERBOSE(format("Empty block pos {} lod {} time {}", ob.position, int(ob.lod),
		// 		Time::get_singleton()->get_ticks_msec()));
		return;
	}

	if (block == nullptr) {
		block = memnew(VoxelMeshBlockVLT(ob.position, get_mesh_block_size(), ob.lod));
		block->active = active;
		block->set_visible(active);
		mesh_map.set_block(ob.position, block);
		// ZN_PRINT_VERBOSE(format("Created block pos {} lod {} time {}", ob.position, int(ob.lod),
		// 		Time::get_singleton()->get_ticks_msec()));
	}

	bool has_collision = get_generate_collisions();
	if (has_collision && _collision_lod_count != 0) {
		has_collision = ob.lod < _collision_lod_count;
	}

	// TODO Is this boolean needed anymore now that we create blocks only if a surface is present?
	if (block->got_first_mesh_update == false) {
		block->got_first_mesh_update = true;

		// TODO Need a more generic API for this kind of stuff
		if (_instancer != nullptr && ob.surfaces.surfaces.size() > 0) {
			// TODO The mesh could come from an edited region!
			// We would have to know if specific voxels got edited, or different from the generator
			_instancer->on_mesh_block_enter(ob.position, ob.lod, ob.surfaces.surfaces[0].arrays);
		}

		// Lazy initialization

		// print_line(String("Adding block {0} at lod {1}").format(varray(eo.block_position.to_vec3(), eo.lod)));
		// set_mesh_block_active(*block, false);
		block->set_parent_visible(is_visible());
		block->set_world(get_world_3d());

		if (_shader_material_pool.get_template().is_valid() && block->get_shader_material().is_null()) {
			ZN_PROFILE_SCOPE_NAMED("Add ShaderMaterial");

			// Pooling shader materials is necessary for now, to avoid stuttering in the editor.
			// Due to a signal used to keep the inspector up to date, even though these
			// material copies will never be seen in the inspector
			// See https://github.com/godotengine/godot/issues/34741
			Ref<ShaderMaterial> sm = _shader_material_pool.allocate();

			// Set individual shader material, because each block can have dynamic parameters,
			// used to smooth seams without re-uploading meshes and allow to implement LOD fading
			block->set_shader_material(sm);
		}

		block->set_transition_mask(transition_mask);
	}

	block->set_mesh(mesh, DirectMeshInstance::GIMode(get_gi_mode()),
			RenderingServer::ShadowCastingSetting(get_shadow_casting()));

	if (!ob.has_mesh_resource) {
		// Profiling has shown Godot takes as much time to build a transition mesh as the main mesh of a block, so
		// because there are 6 transition meshes per block, we would spend about 80% of the time on these if we build
		// them all. Which is counter-intuitive because transition meshes are tiny in comparison... (collision meshes
		// still take 5x more time than building ALL rendering meshes but that's a different issue).
		// Therefore I recommend combining them with the main mesh. This code might not do anything now.
		ZN_PROFILE_SCOPE_NAMED("Transition meshes");

		for (unsigned int dir = 0; dir < mesh_data.transition_surfaces.size(); ++dir) {
			Ref<ArrayMesh> transition_mesh = build_mesh(to_span(mesh_data.transition_surfaces[dir]),
					mesh_data.primitive_type, mesh_data.mesh_flags, _material);

			block->set_transition_mesh(transition_mesh, dir, DirectMeshInstance::GIMode(get_gi_mode()),
					RenderingServer::ShadowCastingSetting(get_shadow_casting()));
		}
	}

	if (has_collision) {
		const uint64_t now = get_ticks_msec();

		if (_collision_update_delay == 0 ||
				static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
			ZN_ASSERT(_mesher.is_valid());
			Ref<Shape3D> collision_shape = make_collision_shape_from_mesher_output(ob.surfaces, **_mesher);
			const bool debug_collisions = is_inside_tree() ? get_tree()->is_debugging_collisions_hint() : false;
			block->set_collision_shape(collision_shape, debug_collisions, this, _collision_margin);

			block->set_collision_layer(_collision_layer);
			block->set_collision_mask(_collision_mask);
			block->last_collider_update_time = now;
			block->deferred_collider_data.reset();

		} else {
			if (block->deferred_collider_data == nullptr) {
				_deferred_collision_updates_per_lod[ob.lod].push_back(ob.position);
				block->deferred_collider_data = make_unique_instance<VoxelMesher::Output>();
			}
			*block->deferred_collider_data = std::move(ob.surfaces);
		}
	}

	block->set_parent_transform(get_global_transform());

	if (ob.detail_textures != nullptr) {
		if (ob.detail_textures->valid) {
			apply_detail_texture_update_to_block(*block, *ob.detail_textures, ob.lod);
		} else {
			// Textures aren't ready.
			// To avoid a jarring transitions from "sharp" to "blurry", keep using parent texture if available.
			// We could do this on many other levels (like propagating to children when a normalmap is assigned),
			// but if we have to, it means the calculation is too expensive anyways,
			// so it's usually better to tune it in the first place.
			try_apply_parent_detail_texture_to_block(*block, ob.position);
		}
	}

#ifdef TOOLS_ENABLED
	if (debug_is_draw_enabled() && debug_get_draw_flag(DEBUG_DRAW_MESH_UPDATES)) {
		_debug_mesh_update_items.push_back({ ob.position, ob.lod, DebugMeshUpdateItem::LINGER_FRAMES });
	}
#endif
}

void VoxelLodTerrain::apply_detail_texture_update(VoxelEngine::BlockDetailTextureOutput &ob) {
	VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[ob.lod_index];
	VoxelMeshBlockVLT *block = mesh_map.get_block(ob.position);

	// This can happen if:
	// - Virtual texture rendering results are handled before the first meshing results which that have created the
	//   block. In this case it will be applied when meshing results get handled, since the data is shared with it.
	// - The block was indeed unloaded early.
	if (block == nullptr) {
		// ZN_PRINT_VERBOSE(format("Ignored virtual texture update, block not found. pos {} lod {} time {}",
		// ob.position, 		ob.lod_index, Time::get_singleton()->get_ticks_msec()));
		return;
	}

	ZN_ASSERT_RETURN(ob.detail_textures != nullptr);
	ZN_ASSERT_RETURN(ob.detail_textures->valid);

	apply_detail_texture_update_to_block(*block, *ob.detail_textures, ob.lod_index);
}

static void try_apply_parent_virtual_texture_to_block(VoxelMeshBlockVLT &block, Vector3i bpos, ShaderMaterial &material,
		unsigned int mesh_block_size, const VoxelMeshBlockVLT &parent_block, Vector3i parent_bpos,
		const DetailRenderingSettings &detail_texture_settings) {
	//
	Ref<ShaderMaterial> parent_material = parent_block.get_shader_material();
	ZN_ASSERT_RETURN(parent_material.is_valid());
	const VoxelStringNames &sn = VoxelStringNames::get_singleton();
	Ref<Texture2D> cell_lookup_texture = parent_material->get_shader_parameter(sn.u_voxel_cell_lookup);
	if (cell_lookup_texture.is_null()) {
		return;
	}
	Ref<Texture2D> normalmap_atlas_texture = parent_material->get_shader_parameter(sn.u_voxel_normalmap_atlas);
	ZN_ASSERT_RETURN(normalmap_atlas_texture.is_valid());

	material.set_shader_parameter(sn.u_voxel_normalmap_atlas, normalmap_atlas_texture);
	material.set_shader_parameter(sn.u_voxel_cell_lookup, cell_lookup_texture);

	const int cell_size = 1 << block.lod_index;
	material.set_shader_parameter(sn.u_voxel_cell_size, cell_size);

	material.set_shader_parameter(sn.u_voxel_block_size, mesh_block_size);

	const Vector4 parent_offset_and_scale =
			parent_material->get_shader_parameter(sn.u_voxel_virtual_texture_offset_scale);
	const Vector3i parent_offset(parent_offset_and_scale.x, parent_offset_and_scale.y, parent_offset_and_scale.z);
	const int fallback_level = parent_block.virtual_texture_fallback_level + 1;

	const Vector3i offset = parent_offset + (bpos - (parent_bpos * 2)) * (mesh_block_size >> fallback_level);
	const float scale = 1.f / float(1 << fallback_level);
	material.set_shader_parameter(
			sn.u_voxel_virtual_texture_offset_scale, Vector4(offset.x, offset.y, offset.z, scale));

	material.set_shader_parameter(sn.u_voxel_virtual_texture_fade, 1.f);

	const unsigned int parent_lod_index = block.lod_index + 1;
	const unsigned int tile_size =
			get_detail_texture_tile_resolution_for_lod(detail_texture_settings, parent_lod_index);
	material.set_shader_parameter(sn.u_voxel_virtual_texture_tile_size, tile_size);

	block.virtual_texture_fallback_level = fallback_level;
}

void VoxelLodTerrain::try_apply_parent_detail_texture_to_block(VoxelMeshBlockVLT &block, Vector3i bpos) {
	ZN_PROFILE_SCOPE();

	Ref<ShaderMaterial> material = block.get_shader_material();
	if (!material.is_valid()) {
		return;
	}
	if (block.lod_index == get_lod_count()) {
		return;
	}

	// Only looking up one level for now
	const unsigned int parent_lod_index = block.lod_index + 1;
	const VoxelMeshMap<VoxelMeshBlockVLT> &parent_map = _mesh_maps_per_lod[parent_lod_index];
	const Vector3i parent_bpos = bpos >> 1;
	const VoxelMeshBlockVLT *parent_block = parent_map.get_block(parent_bpos);
	if (parent_block == nullptr) {
		return;
	}

	zylann::voxel::try_apply_parent_virtual_texture_to_block(block, bpos, **material, get_mesh_block_size(),
			*parent_block, parent_bpos, _update_data->settings.detail_texture_settings);
}

void VoxelLodTerrain::apply_detail_texture_update_to_block(
		VoxelMeshBlockVLT &block, DetailTextureOutput &ob, unsigned int lod_index) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(ob.valid);

	DetailTextures normalmap_textures = ob.textures;

	if (normalmap_textures.lookup.is_null()) {
		// Textures couldn't be created in VRAM so far, do it now. (OpenGL/low-end?)
		// TODO When this code path is required, use a time-spread task to reduce stalls
		DetailImages normalmap_images = ob.images;
		normalmap_textures = store_normalmap_data_to_textures(normalmap_images);
	}

	Ref<ShaderMaterial> material = block.get_shader_material();
	if (material.is_valid()) {
		const VoxelStringNames &sn = VoxelStringNames::get_singleton();

		const bool had_texture = material->get_shader_parameter(sn.u_voxel_cell_lookup) != Variant();
		material->set_shader_parameter(sn.u_voxel_normalmap_atlas, normalmap_textures.atlas);
		material->set_shader_parameter(sn.u_voxel_cell_lookup, normalmap_textures.lookup);
		const int cell_size = 1 << lod_index;
		material->set_shader_parameter(sn.u_voxel_cell_size, cell_size);
		material->set_shader_parameter(sn.u_voxel_block_size, get_mesh_block_size());
		material->set_shader_parameter(sn.u_voxel_virtual_texture_offset_scale, Vector4(0, 0, 0, 1));

		if (!had_texture) {
			if (_lod_fade_duration > 0.f) {
				// Fade-in to reduce "popping" details
				_fading_detail_textures.push_back(FadingDetailTexture{ block.position, lod_index, 0.f });
				material->set_shader_parameter(sn.u_voxel_virtual_texture_fade, 0.f);
			} else {
				material->set_shader_parameter(sn.u_voxel_virtual_texture_fade, 1.f);
			}
		}

		// We may set this again in case the material was using textures from the parent LOD as a temporary fallback
		const unsigned int tile_size =
				get_detail_texture_tile_resolution_for_lod(_update_data->settings.detail_texture_settings, lod_index);
		material->set_shader_parameter(sn.u_voxel_virtual_texture_tile_size, tile_size);
	}
	// If the material is not valid... well it means the user hasn't set up one, so all the hardwork of making these
	// textures goes in the bin. That should be a warning in the editor.

	{
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
		RWLockRead rlock(lod.mesh_map_state.map_lock);
		auto mesh_block_state_it = lod.mesh_map_state.map.find(block.position);
		if (mesh_block_state_it != lod.mesh_map_state.map.end()) {
			VoxelLodTerrainUpdateData::VirtualTextureState expected_vt_state =
					VoxelLodTerrainUpdateData::VIRTUAL_TEXTURE_PENDING;
			// If it was PENDING, set it to IDLE.
			mesh_block_state_it->second.virtual_texture_state.compare_exchange_strong(
					expected_vt_state, VoxelLodTerrainUpdateData::VIRTUAL_TEXTURE_IDLE);
			// TODO If the mesh was modified again since, we need to schedule an extra update for the virtual texture to
			// catch up. But for now I'm not sure if there is much value in doing so. It can get updated by the next
			// edit. Scheduling an update from here isn't mildly inconvenient due to threading.
		}
	}

	block.virtual_texture_fallback_level = 0;
}

void VoxelLodTerrain::process_deferred_collision_updates(uint32_t timeout_msec) {
	ZN_PROFILE_SCOPE();

	const unsigned int lod_count = get_lod_count();
	// TODO We may move this in a time spread task somehow, the timeout does not account for them so could take longer
	const uint64_t then = get_ticks_msec();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		std::vector<Vector3i> &deferred_collision_updates = _deferred_collision_updates_per_lod[lod_index];

		for (unsigned int i = 0; i < deferred_collision_updates.size(); ++i) {
			const Vector3i block_pos = deferred_collision_updates[i];
			VoxelMeshBlockVLT *block = mesh_map.get_block(block_pos);

			if (block == nullptr || block->deferred_collider_data == nullptr) {
				// Block was unloaded or no longer needs a collision update
				unordered_remove(deferred_collision_updates, i);
				--i;
				continue;
			}

			const uint64_t now = get_ticks_msec();

			if (static_cast<int>(now - block->last_collider_update_time) > _collision_update_delay) {
				Ref<Shape3D> collision_shape;
				if (_mesher.is_valid()) {
					collision_shape =
							make_collision_shape_from_mesher_output(*block->deferred_collider_data, **_mesher);
				}

				block->set_collision_shape(
						collision_shape, get_tree()->is_debugging_collisions_hint(), this, _collision_margin);
				block->set_collision_layer(_collision_layer);
				block->set_collision_mask(_collision_mask);
				block->last_collider_update_time = now;
				block->deferred_collider_data.reset();

				unordered_remove(deferred_collision_updates, i);
				--i;
			}

			// We always process at least one, then we check the timeout
			if (get_ticks_msec() - then >= timeout_msec) {
				return;
			}
		}
	}
}

void VoxelLodTerrain::abort_async_edits() {
	_update_data->wait_for_end_of_task();
	VoxelLodTerrainUpdateData::State &state = _update_data->state;

	for (auto it = state.pending_async_edits.begin(); it != state.pending_async_edits.end(); ++it) {
		VoxelLodTerrainUpdateData::AsyncEdit &e = *it;
		CRASH_COND(e.task == nullptr);
		memdelete(e.task);
	}
	state.pending_async_edits.clear();
	state.running_async_edits.clear();
	// Can't cancel edits which are already running on the thread pool,
	// so the caller of this function must ensure none of them are running, or none will have an effect
}

void VoxelLodTerrain::process_fading_blocks(float delta) {
	ZN_PROFILE_SCOPE();

	const float speed = _lod_fade_duration < 0.001f ? 99999.f : delta / _lod_fade_duration;

	{
		ZN_PROFILE_SCOPE();
		for (unsigned int lod_index = 0; lod_index < _fading_blocks_per_lod.size(); ++lod_index) {
			std::map<Vector3i, VoxelMeshBlockVLT *> &fading_blocks = _fading_blocks_per_lod[lod_index];
			std::map<Vector3i, VoxelMeshBlockVLT *>::iterator it = fading_blocks.begin();

			while (it != fading_blocks.end()) {
				VoxelMeshBlockVLT *block = it->second;
				ZN_ASSERT(block != nullptr);
				// The collection of fading blocks must only contain fading blocks
				ERR_FAIL_COND(block->fading_state == VoxelMeshBlockVLT::FADING_NONE);

				const bool finished = block->update_fading(speed);

				if (finished) {
					// `erase` returns the next iterator
					it = fading_blocks.erase(it);

				} else {
					++it;
				}
			}
		}
	}

	{
		ZN_PROFILE_SCOPE();
		// ZN_PROFILE_PLOT("fading_out_meshes", int64_t(_fading_out_meshes.size()));
		for (unsigned int i = 0; i < _fading_out_meshes.size();) {
			FadingOutMesh &item = _fading_out_meshes[i];
			item.progress -= speed;
			if (item.progress <= 0.f) {
				_shader_material_pool.recycle(item.shader_material);
				// TODO Optimize: mesh instances destroyed here can be really slow due to materials...
				// Profiling has shown that `RendererSceneCull::free` of a mesh instance
				// leads to `RendererRD::MaterialStorage::_update_queued_materials()` to be called, which internally
				// updates hundreds of materials (supposedly from every block). Can take 1ms for a single instance,
				// while the rest of the work is barely 1%! Why is Godot doing this? I tried resetting the material like
				// with blocks, but that didn't improve anything...
				// item.mesh_instance.set_material_override(Ref<Material>());
				_fading_out_meshes[i] = std::move(_fading_out_meshes.back());
				_fading_out_meshes.pop_back();
			} else {
				item.shader_material->set_shader_parameter(
						VoxelStringNames::get_singleton().u_lod_fade, Vector2(1.f - item.progress, 0.f));
				++i;
			}
		}
	}

	{
		ZN_PROFILE_SCOPE();
		const unsigned int lod_count = get_lod_count();

		for (unsigned int i = 0; i < _fading_detail_textures.size();) {
			FadingDetailTexture &item = _fading_detail_textures[i];
			bool remove = true;

			if (item.lod_index < lod_count) {
				VoxelMeshBlockVLT *block = _mesh_maps_per_lod[item.lod_index].get_block(item.block_position);

				if (block != nullptr) {
					Ref<ShaderMaterial> sm = block->get_shader_material();

					if (sm.is_valid()) {
						item.progress = math::min(item.progress + speed, 1.f);
						remove = item.progress >= 1.f;
						sm->set_shader_parameter(
								VoxelStringNames::get_singleton().u_voxel_virtual_texture_fade, item.progress);
					}
				}
			}

			if (remove) {
				_fading_detail_textures[i] = _fading_detail_textures.back();
				_fading_detail_textures.pop_back();
			} else {
				++i;
			}
		}
	}
}

VoxelLodTerrain::LocalCameraInfo VoxelLodTerrain::get_local_camera_info() const {
	LocalCameraInfo info;
	if (!is_inside_tree()) {
		return info;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// Falling back on the editor's camera
		info.position = gd::VoxelEngine::get_singleton()->get_editor_camera_position();
		info.forward = gd::VoxelEngine::get_singleton()->get_editor_camera_direction();
		return info;
	}
#endif
	const Viewport *vp = get_viewport();
	if (vp == nullptr) {
		return info;
	}
	const Camera3D *camera = vp->get_camera_3d();
	if (camera == nullptr) {
		return info;
	}
	Transform3D trans = camera->get_global_transform();
	info.forward = get_forward(trans);
	info.position = trans.get_origin();
	return info;
}

void VoxelLodTerrain::set_instancer(VoxelInstancer *instancer) {
	if (_instancer != nullptr && instancer != nullptr) {
		ERR_FAIL_COND_MSG(_instancer != nullptr, "No more than one VoxelInstancer per terrain");
	}
	_instancer = instancer;
}

// This function is primarily intended for editor use cases at the moment.
// It will be slower than using the instancing generation events,
// because it has to query VisualServer, which then allocates and decodes vertex buffers (assuming they are cached).
Array VoxelLodTerrain::get_mesh_block_surface(Vector3i block_pos, int lod_index) const {
	ZN_PROFILE_SCOPE();

	const int lod_count = get_lod_count();
	ERR_FAIL_COND_V(lod_index < 0 || lod_index >= lod_count, Array());

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

	Ref<Mesh> mesh;
	{
		const VoxelMeshBlockVLT *block = mesh_map.get_block(block_pos);
		if (block != nullptr) {
			mesh = block->get_mesh();
		}
	}

	if (mesh.is_valid()) {
		return mesh->surface_get_arrays(0);
	}

	return Array();
}

void VoxelLodTerrain::get_meshed_block_positions_at_lod(int lod_index, std::vector<Vector3i> &out_positions) const {
	const int lod_count = get_lod_count();
	ERR_FAIL_COND(lod_index < 0 || lod_index >= lod_count);

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

	mesh_map.for_each_block([&out_positions](const VoxelMeshBlockVLT &block) {
		if (block.has_mesh()) {
			out_positions.push_back(block.position);
		}
	});
}

void VoxelLodTerrain::save_all_modified_blocks(bool with_copy) {
	ZN_PROFILE_SCOPE();

	// This is often called before quitting the game or forcing a global save.
	// This could be part of the update task if async, but here we want it to be immediate.
	_update_data->wait_for_end_of_task();

	VoxelLodTerrainUpdateTask::flush_pending_lod_edits(_update_data->state, *_data, get_mesh_block_size());

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();
	std::vector<VoxelData::BlockToSave> blocks_to_save;

	Ref<VoxelStream> stream = get_stream();
	if (stream.is_valid()) {
		// That may cause a stutter, so should be used when the player won't notice
		_data->consume_all_modifications(blocks_to_save, with_copy);

		if (_instancer != nullptr && stream->supports_instance_blocks()) {
			_instancer->save_all_modified_blocks(task_scheduler, nullptr);
		}
	}

	// And flush immediately
	VoxelLodTerrainUpdateTask::send_block_save_requests(
			_volume_id, to_span(blocks_to_save), _streaming_dependency, get_data_block_size(), task_scheduler);
	task_scheduler.flush();
}

const VoxelLodTerrain::Stats &VoxelLodTerrain::get_stats() const {
	return _stats;
}

Dictionary VoxelLodTerrain::_b_get_statistics() const {
	Dictionary d;

	// const unsigned int lod_count = _update_data->settings.lod_count;

	// int deferred_collision_updates = 0;
	// for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
	// 	deferred_collision_updates += _deferred_collision_updates_per_lod[lod_index].size();
	// }

	// Breakdown of information and time spent in _process and the update task.

	// Update task
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_io_requests"] = _stats.time_io_requests;
	d["time_mesh_requests"] = _stats.time_mesh_requests;
	d["time_update_task"] = _stats.time_update_task;
	d["blocked_lods"] = _stats.blocked_lods;

	// Process
	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;

	return d;
}

void VoxelLodTerrain::set_run_stream_in_editor(bool enable) {
	if (enable == _update_data->settings.run_stream_in_editor) {
		return;
	}

	_update_data->wait_for_end_of_task();
	_update_data->settings.run_stream_in_editor = enable;

	if (Engine::get_singleton()->is_editor_hint()) {
		if (enable) {
			_on_stream_params_changed();

		} else {
			// This is expected to block the main thread until the streaming thread is done.
			stop_streamer();
		}
	}
}

bool VoxelLodTerrain::is_stream_running_in_editor() const {
	return _update_data->settings.run_stream_in_editor;
}

void VoxelLodTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelLodTerrain::remesh_all_blocks() {
	// Requests a new mesh for all mesh blocks, without dropping everything first
	_update_data->wait_for_end_of_task();
	const unsigned int lod_count = get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
		for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
			VoxelLodTerrainUpdateTask::schedule_mesh_update(it->second, it->first, lod.blocks_pending_update);
		}
	}
}

void VoxelLodTerrain::set_voxel_bounds(Box3i p_box) {
	_update_data->wait_for_end_of_task();
	Box3i bounds_in_voxels =
			p_box.clipped(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));
	// Round to octree size
	const int octree_size = get_mesh_block_size() << (get_lod_count() - 1);
	bounds_in_voxels = bounds_in_voxels.snapped(octree_size);
	// Can't have a smaller region than one octree
	for (unsigned i = 0; i < Vector3iUtil::AXIS_COUNT; ++i) {
		if (bounds_in_voxels.size[i] < octree_size) {
			bounds_in_voxels.size[i] = octree_size;
		}
	}
	_data->set_bounds(bounds_in_voxels);
	_update_data->state.force_update_octrees_next_update = true;
}

void VoxelLodTerrain::set_collision_update_delay(int delay_msec) {
	_collision_update_delay = math::clamp(delay_msec, 0, 4000);
}

int VoxelLodTerrain::get_collision_update_delay() const {
	return _collision_update_delay;
}

void VoxelLodTerrain::set_lod_fade_duration(float seconds) {
	_lod_fade_duration = math::clamp(seconds, 0.f, 1.f);
}

float VoxelLodTerrain::get_lod_fade_duration() const {
	return _lod_fade_duration;
}

void VoxelLodTerrain::set_normalmap_enabled(bool enable) {
	_update_data->settings.detail_texture_settings.enabled = enable;
}

bool VoxelLodTerrain::is_normalmap_enabled() const {
	return _update_data->settings.detail_texture_settings.enabled;
}

void VoxelLodTerrain::set_normalmap_tile_resolution_min(int resolution) {
	_update_data->settings.detail_texture_settings.tile_resolution_min = math::clamp(resolution, 1, 128);
}

int VoxelLodTerrain::get_normalmap_tile_resolution_min() const {
	return _update_data->settings.detail_texture_settings.tile_resolution_min;
}

void VoxelLodTerrain::set_normalmap_tile_resolution_max(int resolution) {
	_update_data->settings.detail_texture_settings.tile_resolution_max = math::clamp(resolution, 1, 128);
}

int VoxelLodTerrain::get_normalmap_tile_resolution_max() const {
	return _update_data->settings.detail_texture_settings.tile_resolution_max;
}

void VoxelLodTerrain::set_normalmap_begin_lod_index(int lod_index) {
	ERR_FAIL_INDEX(lod_index, int(constants::MAX_LOD));
	_update_data->settings.detail_texture_settings.begin_lod_index = lod_index;
}

int VoxelLodTerrain::get_normalmap_begin_lod_index() const {
	return _update_data->settings.detail_texture_settings.begin_lod_index;
}

void VoxelLodTerrain::set_normalmap_max_deviation_degrees(int angle) {
	_update_data->settings.detail_texture_settings.max_deviation_degrees = math::clamp(angle,
			int(DetailRenderingSettings::MIN_DEVIATION_DEGREES), int(DetailRenderingSettings::MAX_DEVIATION_DEGREES));
}

int VoxelLodTerrain::get_normalmap_max_deviation_degrees() const {
	return _update_data->settings.detail_texture_settings.max_deviation_degrees;
}

void VoxelLodTerrain::set_octahedral_normal_encoding(bool enable) {
	_update_data->settings.detail_texture_settings.octahedral_encoding_enabled = enable;
}

bool VoxelLodTerrain::get_octahedral_normal_encoding() const {
	return _update_data->settings.detail_texture_settings.octahedral_encoding_enabled;
}

void VoxelLodTerrain::set_normalmap_generator_override(Ref<VoxelGenerator> generator_override) {
	_update_data->wait_for_end_of_task();
	_update_data->settings.detail_texture_generator_override = generator_override;
}

Ref<VoxelGenerator> VoxelLodTerrain::get_normalmap_generator_override() const {
	return _update_data->settings.detail_texture_generator_override;
}

void VoxelLodTerrain::set_normalmap_generator_override_begin_lod_index(int lod_index) {
	ERR_FAIL_COND(lod_index < 0);
	ERR_FAIL_COND(lod_index > static_cast<int>(constants::MAX_LOD));
	_update_data->settings.virtual_texture_generator_override_begin_lod_index = lod_index;
}

int VoxelLodTerrain::get_normalmap_generator_override_begin_lod_index() const {
	return _update_data->settings.virtual_texture_generator_override_begin_lod_index;
}

void VoxelLodTerrain::set_normalmap_use_gpu(bool enabled) {
	_update_data->settings.virtual_textures_use_gpu = enabled;
	update_configuration_warnings();
}

bool VoxelLodTerrain::get_normalmap_use_gpu() const {
	return _update_data->settings.virtual_textures_use_gpu;
}

#ifdef TOOLS_ENABLED

void VoxelLodTerrain::get_configuration_warnings(PackedStringArray &warnings) const {
	VoxelNode::get_configuration_warnings(warnings);
	if (!warnings.is_empty()) {
		return;
	}

	Ref<VoxelMesher> mesher = get_mesher();

	// Material
	Ref<ShaderMaterial> shader_material = _material;
	if (shader_material.is_valid() && shader_material->get_shader().is_null()) {
		warnings.append(ZN_TTR("The assigned {0} has no shader").format(varray(ShaderMaterial::get_class_static())));
	}

	if (mesher.is_valid()) {
		// LOD support in mesher
		if (!mesher->supports_lod()) {
			warnings.append(ZN_TTR(
					"The assigned mesher ({0}) does not support level of detail (LOD), results may be unexpected.")
									.format(varray(mesher->get_class())));
		}

		// LOD support in shader
		if (_material.is_valid() && mesher->get_default_lod_material().is_valid()) {
			if (shader_material.is_null()) {
				warnings.append(ZN_TTR(
						"The current mesher ({0}) requires custom shader code to render properly. The current "
						"material might not be appropriate. Hint: you can assign a newly created {1} to fork the "
						"default shader.")
										.format(varray(mesher->get_class(), ShaderMaterial::get_class_static())));
			} else {
				Ref<Shader> shader = shader_material->get_shader();
				if (shader.is_valid()) {
					if (!shader_has_uniform(**shader, VoxelStringNames::get_singleton().u_transition_mask)) {
						warnings.append(ZN_TTR(
								"The current mesher ({0}) requires to use shader with specific uniforms. Missing: {1}")
												.format(varray(mesher->get_class(),
														VoxelStringNames::get_singleton().u_transition_mask)));
					}
				}
			}
		}

		// LOD fading
		if (get_lod_fade_duration() > 0.f) {
			if (shader_material.is_null()) {
				warnings.append(String("Lod fading is enabled but it requires a {0} to render properly.")
										.format(varray(ShaderMaterial::get_class_static())));
			} else {
				Ref<Shader> shader = shader_material->get_shader();
				if (shader.is_null()) {
					warnings.append(String("Lod fading is enabled but the current material is missing a shader.")
											.format(varray(ShaderMaterial::get_class_static())));
				} else {
					if (!shader_has_uniform(**shader, VoxelStringNames::get_singleton().u_lod_fade)) {
						warnings.append(ZN_TTR(
								"Lod fading is enabled but it requires to use a specific shader uniform. Missing: {0}")
												.format(varray(VoxelStringNames::get_singleton().u_lod_fade)));
					}
				}
			}
		}

		// Virtual textures
		Ref<VoxelGenerator> generator = get_generator();
		if (generator.is_valid()) {
			if (is_normalmap_enabled()) {
				if (!generator->supports_series_generation()) {
					warnings.append(ZN_TTR(
							"Normalmaps are enabled, but it requires the generator to be able to generate series of "
							"positions with `generate_series`. The current generator ({0}) does not support it.")
											.format(varray(generator->get_class())));
				}

				if ((generator->get_used_channels_mask() & (1 << VoxelBufferInternal::CHANNEL_SDF)) == 0) {
					warnings.append(ZN_TTR("Normalmaps are enabled, but it requires the generator to use the SDF "
										   "channel. The current generator ({0}) does not support it, or is not "
										   "configured to do so.")
											.format(varray(generator->get_class())));
				}

				if (shader_material.is_valid()) {
					Ref<Shader> shader = shader_material->get_shader();

					if (shader.is_valid()) {
						FixedArray<StringName, 2> expected_uniforms;
						expected_uniforms[0] = VoxelStringNames::get_singleton().u_voxel_normalmap_atlas;
						expected_uniforms[1] = VoxelStringNames::get_singleton().u_voxel_cell_lookup;
						// There is more but they are not absolutely required for the shader to be made working

						const String missing_uniforms = get_missing_uniform_names(to_span(expected_uniforms), **shader);

						if (missing_uniforms.length() != 0) {
							warnings.append(String(
									ZN_TTR("Normalmaps are enabled, but it requires to use a {0} with a shader having "
										   "specific uniforms. Missing ones: {1}"))
													.format(varray(
															ShaderMaterial::get_class_static(), missing_uniforms)));
						}
					}
				}

				if (get_normalmap_use_gpu() && !generator->supports_shaders()) {
					warnings.append(ZN_TTR("Normalmaps are enabled with the option to use the GPU, but the current "
										   "generator ({0}) does not support GLSL.")
											.format(varray(generator->get_class())));
				}
			}
		}
	}
}

#endif // TOOLS_ENABLED

void VoxelLodTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

void VoxelLodTerrain::_b_set_voxel_bounds(AABB aabb) {
	ERR_FAIL_COND(!math::is_valid_size(aabb.size));
	set_voxel_bounds(Box3i(math::round_to_int(aabb.position), math::round_to_int(aabb.size)));
}

AABB VoxelLodTerrain::_b_get_voxel_bounds() const {
	const Box3i b = get_voxel_bounds();
	return AABB(b.pos, b.size);
}

// DEBUG LAND

Array VoxelLodTerrain::debug_raycast_mesh_block(Vector3 world_origin, Vector3 world_direction) const {
	const Transform3D world_to_local = get_global_transform().affine_inverse();
	Vector3 pos = world_to_local.xform(world_origin);
	const Vector3 dir = world_to_local.basis.xform(world_direction);
	const float max_distance = 256;
	const float step = 2.f;
	float distance = 0.f;
	const unsigned int lod_count = get_lod_count();
	const unsigned int mesh_block_size_po2 = _update_data->settings.mesh_block_size_po2;

	Array hits;
	while (distance < max_distance && hits.size() == 0) {
		const Vector3i posi = math::floor_to_int(pos);
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
			const Vector3i bpos = (posi << mesh_block_size_po2) >> lod_index;
			const VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);
			if (block != nullptr && block->is_visible() && block->has_mesh()) {
				Dictionary d;
				d["position"] = block->position;
				d["lod"] = block->lod_index;
				hits.append(d);
			}
		}
		distance += step;
		pos += dir * step;
	}

	return hits;
}

Dictionary VoxelLodTerrain::debug_get_data_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];

	int loading_state = 0;

	Vector3i bpos = math::floor_to_int(fbpos);
	const bool has_block = _data->has_block(bpos, lod_index);

	if (has_block) {
		loading_state = 2;
	} else {
		MutexLock lock(lod.loading_blocks_mutex);
		if (lod.has_loading_block(bpos)) {
			loading_state = 1;
		}
	}

	d["loading_state"] = loading_state;
	return d;
}

Dictionary VoxelLodTerrain::debug_get_mesh_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	const int lod_count = get_lod_count();
	ERR_FAIL_COND_V(lod_index >= lod_count, d);

	const Vector3i bpos = math::floor_to_int(fbpos);

	bool loaded = false;
	bool meshed = false;
	bool visible = false;
	bool active = false;
	int mesh_state = VoxelLodTerrainUpdateData::MESH_NEVER_UPDATED;

	const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
	const VoxelMeshBlockVLT *block = mesh_map.get_block(bpos);

	if (block != nullptr) {
		int recomputed_transition_mask;
		{
			const VoxelLodTerrainUpdateData::Lod &lod = _update_data->state.lods[lod_index];
			RWLockRead rlock(lod.mesh_map_state.map_lock);
			recomputed_transition_mask = VoxelLodTerrainUpdateTask::get_transition_mask(
					_update_data->state, bpos, block->lod_index, lod_count);
			auto it = lod.mesh_map_state.map.find(bpos);
			if (it != lod.mesh_map_state.map.end()) {
				mesh_state = it->second.state;
			}
		}

		loaded = true;
		meshed = block->has_mesh();
		visible = block->is_visible();
		active = block->active;
		d["transition_mask"] = block->get_transition_mask();
		// This can highlight possible bugs between the current state and what it should be
		d["recomputed_transition_mask"] = recomputed_transition_mask;
	}

	d["loaded"] = loaded;
	d["meshed"] = meshed;
	d["mesh_state"] = mesh_state;
	d["visible"] = visible;
	d["active"] = active;
	return d;
}

Array VoxelLodTerrain::debug_get_octree_positions() const {
	_update_data->wait_for_end_of_task();
	Array positions;
	const std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem> &octrees = _update_data->state.lod_octrees;
	positions.resize(octrees.size());
	int i = 0;
	for (auto it = octrees.begin(); it != octrees.end(); ++it) {
		positions[i++] = it->first;
	}
	return positions;
}

Array VoxelLodTerrain::debug_get_octrees_detailed() const {
	// [
	//     Vector3,
	//     Octree,
	//     ...
	// ]
	// Octree [
	//     state: State,
	//     Octree[8] or null
	// ]
	// State {
	//     0: no block
	//     1: no mesh
	//     2: mesh
	// }

	struct L {
		static void read_node(const LodOctree &octree, const LodOctree::Node *node, Vector3i position, int lod_index,
				const VoxelLodTerrainUpdateData::State &state, Array &out_data) {
			ERR_FAIL_COND(lod_index < 0);
			Variant node_state;

			const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			auto mesh_block_it = lod.mesh_map_state.map.find(position);
			if (mesh_block_it == lod.mesh_map_state.map.end()) {
				node_state = 0;
			} else {
				if (mesh_block_it->second.state == VoxelLodTerrainUpdateData::MESH_UP_TO_DATE) {
					node_state = 2;
				} else {
					node_state = 1;
				}
			}

			out_data.append(node_state);

			if (node->has_children()) {
				Array children_data;
				for (unsigned int i = 0; i < 8; ++i) {
					Array child_data;
					const LodOctree::Node *child = octree.get_child(node, i);
					const Vector3i child_pos = LodOctree::get_child_position(position, i);
					read_node(octree, child, child_pos, lod_index - 1, state, child_data);
					children_data.append(child_data);
				}
				out_data.append(children_data);

			} else {
				out_data.append(Variant());
			}
		}
	};

	_update_data->wait_for_end_of_task();

	const std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem> &octrees = _update_data->state.lod_octrees;

	Array forest_data;

	for (auto it = octrees.begin(); it != octrees.end(); ++it) {
		const LodOctree &octree = it->second.octree;
		const LodOctree::Node *root = octree.get_root();
		Array root_data;
		const Vector3i octree_pos = it->first;
		L::read_node(octree, root, octree_pos, get_lod_count() - 1, _update_data->state, root_data);
		forest_data.append(octree_pos);
		forest_data.append(root_data);
	}

	return forest_data;
}

void VoxelLodTerrain::debug_set_draw_enabled(bool enabled) {
#ifdef TOOLS_ENABLED
	_debug_draw_enabled = enabled;
	if (_debug_draw_enabled) {
		if (is_inside_tree()) {
			_debug_renderer.set_world(is_visible_in_tree() ? *get_world_3d() : nullptr);
		}
	} else {
		_debug_renderer.clear();
		_debug_mesh_update_items.clear();
		_debug_edit_items.clear();
	}
#endif
}

bool VoxelLodTerrain::debug_is_draw_enabled() const {
#ifdef TOOLS_ENABLED
	return _debug_draw_enabled;
#else
	return false;
#endif
}

void VoxelLodTerrain::debug_set_draw_flag(DebugDrawFlag flag_index, bool enabled) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_INDEX(flag_index, DEBUG_DRAW_FLAGS_COUNT);
	if (enabled) {
		_debug_draw_flags |= (1 << flag_index);
	} else {
		_debug_draw_flags &= ~(1 << flag_index);
	}
#endif
}

bool VoxelLodTerrain::debug_get_draw_flag(DebugDrawFlag flag_index) const {
#ifdef TOOLS_ENABLED
	ERR_FAIL_INDEX_V(flag_index, DEBUG_DRAW_FLAGS_COUNT, false);
	return (_debug_draw_flags & (1 << flag_index)) != 0;
#else
	return false;
#endif
}

#ifdef TOOLS_ENABLED

void VoxelLodTerrain::update_gizmos() {
	ZN_PROFILE_SCOPE();

	// Hopefully this should not be skipped most of the time, because the task is started at the end of `_process`,
	// and gizmos update before. So the task has about 16ms to complete. If it takes longer, it will skip.
	// This allows us to avoid locking data structures.
	if (_update_data->task_is_complete == false) {
		return;
	}

	const VoxelLodTerrainUpdateData::State &state = _update_data->state;

	DebugRenderer &dr = _debug_renderer;
	dr.begin();

	const Transform3D parent_transform = get_global_transform();
	const unsigned int lod_count = get_lod_count();
	const int mesh_block_size = get_mesh_block_size();

	// Octree bounds
	if (debug_get_draw_flag(DEBUG_DRAW_OCTREE_BOUNDS)) {
		const int octree_size = 1 << LodOctree::get_octree_size_po2(get_mesh_block_size_pow2(), get_lod_count());
		const Basis local_octree_basis = Basis().scaled(Vector3(octree_size, octree_size, octree_size));

		for (auto it = state.lod_octrees.begin(); it != state.lod_octrees.end(); ++it) {
			const Transform3D local_transform(local_octree_basis, it->first * octree_size);
			dr.draw_box(parent_transform * local_transform, DebugColors::ID_OCTREE_BOUNDS);
		}
	}

	// Volume bounds
	if (debug_get_draw_flag(DEBUG_DRAW_VOLUME_BOUNDS)) {
		const Box3i bounds_in_voxels = get_voxel_bounds();
		const float bounds_in_voxels_len = Vector3(bounds_in_voxels.size).length();

		if (bounds_in_voxels_len < 10000) {
			const Vector3 margin = Vector3(1, 1, 1) * bounds_in_voxels_len * 0.0025f;
			const Vector3 size = bounds_in_voxels.size;
			const Transform3D local_transform(
					Basis().scaled(size + margin * 2.f), Vector3(bounds_in_voxels.pos) - margin);
			dr.draw_box(parent_transform * local_transform, DebugColors::ID_VOXEL_BOUNDS);
		}
	}

	// Octree nodes
	if (debug_get_draw_flag(DEBUG_DRAW_OCTREE_NODES)) {
		// That can be expensive to draw
		const float lod_count_f = lod_count;

		for (auto it = state.lod_octrees.begin(); it != state.lod_octrees.end(); ++it) {
			const LodOctree &octree = it->second.octree;

			const Vector3i block_pos_maxlod = it->first;
			const Vector3i block_offset_lod0 = block_pos_maxlod << (lod_count - 1);

			octree.for_each_leaf([&dr, block_offset_lod0, mesh_block_size, parent_transform, lod_count_f](
										 Vector3i node_pos, int lod_index, const LodOctree::NodeData &node_data) {
				//
				const int size = mesh_block_size << lod_index;
				const Vector3i voxel_pos = mesh_block_size * ((node_pos << lod_index) + block_offset_lod0);
				const Transform3D local_transform(Basis().scaled(Vector3(size, size, size)), voxel_pos);
				const Transform3D t = parent_transform * local_transform;
				// Squaring because lower lod indexes are more interesting to see, so we give them more contrast.
				// Also this might be better with sRGB?
				const float g = math::squared(math::max(1.f - float(lod_index) / lod_count_f, 0.f));
				dr.draw_box_mm(t, Color8(255, uint8_t(g * 254.f), 0, 255));
			});
		}
	}

	// Edited blocks
	if (debug_get_draw_flag(DEBUG_DRAW_EDITED_BLOCKS) && _edited_blocks_gizmos_lod_index < lod_count) {
		const int data_block_size = get_data_block_size() << _edited_blocks_gizmos_lod_index;
		const Basis basis(Basis().scaled(Vector3(data_block_size, data_block_size, data_block_size)));

		_data->for_each_block_at_lod(
				[&dr, parent_transform, data_block_size, basis](const Vector3i &bpos, const VoxelDataBlock &block) {
					if (block.is_edited()) {
						const Transform3D local_transform(basis, bpos * data_block_size);
						const Transform3D t = parent_transform * local_transform;
						const Color8 c = block.is_modified() ? Color8(255, 255, 0, 255) : Color8(0, 255, 0, 255);
						dr.draw_box_mm(t, c);
					}
				},
				_edited_blocks_gizmos_lod_index);
	}

	// Debug updates
	for (unsigned int i = 0; i < _debug_mesh_update_items.size();) {
		DebugMeshUpdateItem &item = _debug_mesh_update_items[i];

		const Transform3D local_transform(Basis().scaled(to_vec3(Vector3iUtil::create(mesh_block_size << item.lod))),
				to_vec3(item.position * (mesh_block_size << item.lod)));

		const Transform3D t = parent_transform * local_transform;

		const Color color = math::lerp(
				Color(0, 0, 0), Color(0, 1, 1), item.remaining_frames / float(DebugMeshUpdateItem::LINGER_FRAMES));
		dr.draw_box_mm(t, Color8(color));

		--item.remaining_frames;
		if (item.remaining_frames == 0) {
			item = _debug_mesh_update_items.back();
			_debug_mesh_update_items.pop_back();
		} else {
			++i;
		}
	}

	for (unsigned int i = 0; i < _debug_edit_items.size();) {
		DebugEditItem &item = _debug_edit_items[i];

		const Transform3D local_transform(Basis().scaled(to_vec3(item.voxel_box.size)), to_vec3(item.voxel_box.pos));

		const Transform3D t = parent_transform * local_transform;

		const Color color = math::lerp(
				Color(0, 0, 0), Color(1, 1, 0), item.remaining_frames / float(DebugMeshUpdateItem::LINGER_FRAMES));
		dr.draw_box_mm(t, Color8(color));

		--item.remaining_frames;
		if (item.remaining_frames == 0) {
			item = _debug_edit_items.back();
			_debug_edit_items.pop_back();
		} else {
			++i;
		}
	}

	dr.end();
}

#endif

// This copies at multiple LOD levels to debug mips
Array VoxelLodTerrain::_b_debug_print_sdf_top_down(Vector3i center, Vector3i extents) {
	ERR_FAIL_COND_V(!math::is_valid_size(extents), Array());

	Array image_array;
	const unsigned int lod_count = get_lod_count();
	const VoxelData &voxel_data = *_data;

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const Box3i world_box = Box3i::from_center_extents(center >> lod_index, extents >> lod_index);

		if (Vector3iUtil::get_volume(world_box.size) == 0) {
			continue;
		}

		VoxelBufferInternal buffer;
		buffer.create(world_box.size);

		world_box.for_each_cell([world_box, &buffer, &voxel_data](const Vector3i &world_pos) {
			const Vector3i rpos = world_pos - world_box.pos;
			VoxelSingleValue v;
			v.f = 1.f;
			v = voxel_data.get_voxel(world_pos, VoxelBufferInternal::CHANNEL_SDF, v);
			buffer.set_voxel_f(v.f, rpos.x, rpos.y, rpos.z, VoxelBufferInternal::CHANNEL_SDF);
		});

		Ref<Image> image = gd::VoxelBuffer::debug_print_sdf_to_image_top_down(buffer);
		image_array.append(image);
	}

	return image_array;
}

int VoxelLodTerrain::_b_debug_get_mesh_block_count() const {
	int sum = 0;
	const unsigned int lod_count = get_lod_count();
	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];
		sum += mesh_map.get_block_count();
	}
	return sum;
}

int VoxelLodTerrain::_b_debug_get_data_block_count() const {
	return _data->get_block_count();
}

Node3D *VoxelLodTerrain::debug_dump_as_nodes(bool include_instancer) const {
	Node3D *root = memnew(Node3D);
	root->set_name(get_name());

	const unsigned int lod_count = get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		const VoxelMeshMap<VoxelMeshBlockVLT> &mesh_map = _mesh_maps_per_lod[lod_index];

		mesh_map.for_each_block([root](const VoxelMeshBlockVLT &block) {
			block.for_each_mesh_instance_with_transform([root, &block](const DirectMeshInstance &dmi, Transform3D t) {
				Ref<Mesh> mesh = dmi.get_mesh();

				if (mesh.is_valid()) {
					MeshInstance3D *mi = memnew(MeshInstance3D);
					mi->set_mesh(mesh);
					mi->set_transform(t);
					// TODO Transition mesh visibility?
					mi->set_visible(block.is_visible());
					root->add_child(mi);
				}
			});
		});
	}

	if (include_instancer && _instancer != nullptr) {
		Node *instances_root = _instancer->debug_dump_as_nodes();
		if (instances_root != nullptr) {
			root->add_child(instances_root);
		}
	}

	return root;
}

int /*Error*/ VoxelLodTerrain::_b_debug_dump_as_scene(String fpath, bool include_instancer) const {
	Node3D *root = debug_dump_as_nodes(include_instancer);
	ZN_ASSERT_RETURN_V(root != nullptr, ERR_BUG);

	set_nodes_owner_except_root(root, root);

	Ref<PackedScene> scene;
	scene.instantiate();
	const Error pack_result = scene->pack(root);
	memdelete(root);
	if (pack_result != OK) {
		return pack_result;
	}

	const Error save_result = save_resource(scene, fpath, ResourceSaver::FLAG_BUNDLE_RESOURCES);
	return save_result;
}

void VoxelLodTerrain::_bind_methods() {
	// Material

	ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelLodTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelLodTerrain::get_material);

	// Bounds

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelLodTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelLodTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("set_voxel_bounds"), &VoxelLodTerrain::_b_set_voxel_bounds);
	ClassDB::bind_method(D_METHOD("get_voxel_bounds"), &VoxelLodTerrain::_b_get_voxel_bounds);

	// Collisions

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelLodTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelLodTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_collision_lod_count"), &VoxelLodTerrain::get_collision_lod_count);
	ClassDB::bind_method(D_METHOD("set_collision_lod_count", "count"), &VoxelLodTerrain::set_collision_lod_count);

	ClassDB::bind_method(D_METHOD("get_collision_layer"), &VoxelLodTerrain::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &VoxelLodTerrain::set_collision_layer);

	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelLodTerrain::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelLodTerrain::set_collision_mask);

	ClassDB::bind_method(D_METHOD("get_collision_margin"), &VoxelLodTerrain::get_collision_margin);
	ClassDB::bind_method(D_METHOD("set_collision_margin", "margin"), &VoxelLodTerrain::set_collision_margin);

	ClassDB::bind_method(D_METHOD("get_collision_update_delay"), &VoxelLodTerrain::get_collision_update_delay);
	ClassDB::bind_method(
			D_METHOD("set_collision_update_delay", "delay_msec"), &VoxelLodTerrain::set_collision_update_delay);

	// LOD

	ClassDB::bind_method(D_METHOD("get_lod_fade_duration"), &VoxelLodTerrain::get_lod_fade_duration);
	ClassDB::bind_method(D_METHOD("set_lod_fade_duration", "seconds"), &VoxelLodTerrain::set_lod_fade_duration);

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("set_lod_distance", "lod_distance"), &VoxelLodTerrain::set_lod_distance);
	ClassDB::bind_method(D_METHOD("get_lod_distance"), &VoxelLodTerrain::get_lod_distance);

	// Misc

	ClassDB::bind_method(D_METHOD("voxel_to_data_block_position", "voxel_position", "lod_index"),
			&VoxelLodTerrain::voxel_to_data_block_position);
	ClassDB::bind_method(D_METHOD("voxel_to_mesh_block_position", "voxel_position", "lod_index"),
			&VoxelLodTerrain::voxel_to_mesh_block_position);

	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelLodTerrain::get_voxel_tool);
	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelLodTerrain::_b_save_modified_blocks);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor"), &VoxelLodTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelLodTerrain::is_stream_running_in_editor);

	// Normalmaps

	ClassDB::bind_method(D_METHOD("set_normalmap_enabled", "enabled"), &VoxelLodTerrain::set_normalmap_enabled);
	ClassDB::bind_method(D_METHOD("is_normalmap_enabled"), &VoxelLodTerrain::is_normalmap_enabled);

	ClassDB::bind_method(D_METHOD("set_normalmap_tile_resolution_min", "resolution"),
			&VoxelLodTerrain::set_normalmap_tile_resolution_min);
	ClassDB::bind_method(
			D_METHOD("get_normalmap_tile_resolution_min"), &VoxelLodTerrain::get_normalmap_tile_resolution_min);

	ClassDB::bind_method(D_METHOD("set_normalmap_tile_resolution_max", "resolution"),
			&VoxelLodTerrain::set_normalmap_tile_resolution_max);
	ClassDB::bind_method(
			D_METHOD("get_normalmap_tile_resolution_max"), &VoxelLodTerrain::get_normalmap_tile_resolution_max);

	ClassDB::bind_method(
			D_METHOD("set_normalmap_begin_lod_index", "lod_index"), &VoxelLodTerrain::set_normalmap_begin_lod_index);
	ClassDB::bind_method(D_METHOD("get_normalmap_begin_lod_index"), &VoxelLodTerrain::get_normalmap_begin_lod_index);

	ClassDB::bind_method(D_METHOD("set_normalmap_max_deviation_degrees", "angle"),
			&VoxelLodTerrain::set_normalmap_max_deviation_degrees);
	ClassDB::bind_method(
			D_METHOD("get_normalmap_max_deviation_degrees"), &VoxelLodTerrain::get_normalmap_max_deviation_degrees);

	ClassDB::bind_method(
			D_METHOD("set_octahedral_normal_encoding", "enabled"), &VoxelLodTerrain::set_octahedral_normal_encoding);
	ClassDB::bind_method(D_METHOD("get_octahedral_normal_encoding"), &VoxelLodTerrain::get_octahedral_normal_encoding);

	ClassDB::bind_method(D_METHOD("set_normalmap_generator_override", "generator_override"),
			&VoxelLodTerrain::set_normalmap_generator_override);
	ClassDB::bind_method(
			D_METHOD("get_normalmap_generator_override"), &VoxelLodTerrain::get_normalmap_generator_override);

	ClassDB::bind_method(D_METHOD("set_normalmap_generator_override_begin_lod_index", "lod_index"),
			&VoxelLodTerrain::set_normalmap_generator_override_begin_lod_index);
	ClassDB::bind_method(D_METHOD("get_normalmap_generator_override_begin_lod_index"),
			&VoxelLodTerrain::get_normalmap_generator_override_begin_lod_index);

	ClassDB::bind_method(D_METHOD("set_normalmap_use_gpu", "enabled"), &VoxelLodTerrain::set_normalmap_use_gpu);
	ClassDB::bind_method(D_METHOD("get_normalmap_use_gpu"), &VoxelLodTerrain::get_normalmap_use_gpu);

	// Advanced

	ClassDB::bind_method(D_METHOD("get_mesh_block_size"), &VoxelLodTerrain::get_mesh_block_size);
	ClassDB::bind_method(D_METHOD("set_mesh_block_size"), &VoxelLodTerrain::set_mesh_block_size);

	ClassDB::bind_method(D_METHOD("get_data_block_size"), &VoxelLodTerrain::get_data_block_size);
	ClassDB::bind_method(D_METHOD("get_data_block_region_extent"), &VoxelLodTerrain::get_data_block_region_extent);

	ClassDB::bind_method(D_METHOD("set_full_load_mode_enabled"), &VoxelLodTerrain::set_full_load_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_full_load_mode_enabled"), &VoxelLodTerrain::is_full_load_mode_enabled);

	ClassDB::bind_method(
			D_METHOD("set_threaded_update_enabled", "enabled"), &VoxelLodTerrain::set_threaded_update_enabled);
	ClassDB::bind_method(D_METHOD("is_threaded_update_enabled"), &VoxelLodTerrain::is_threaded_update_enabled);

	ClassDB::bind_method(D_METHOD("set_process_callback", "mode"), &VoxelLodTerrain::set_process_callback);
	ClassDB::bind_method(D_METHOD("get_process_callback"), &VoxelLodTerrain::get_process_callback);

	// Debug

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelLodTerrain::_b_get_statistics);

	ClassDB::bind_method(
			D_METHOD("debug_raycast_mesh_block", "origin", "dir"), &VoxelLodTerrain::debug_raycast_mesh_block);
	ClassDB::bind_method(
			D_METHOD("debug_get_data_block_info", "block_pos", "lod"), &VoxelLodTerrain::debug_get_data_block_info);
	ClassDB::bind_method(
			D_METHOD("debug_get_mesh_block_info", "block_pos", "lod"), &VoxelLodTerrain::debug_get_mesh_block_info);
	ClassDB::bind_method(D_METHOD("debug_get_octrees_detailed"), &VoxelLodTerrain::debug_get_octrees_detailed);
	ClassDB::bind_method(
			D_METHOD("debug_print_sdf_top_down", "center", "extents"), &VoxelLodTerrain::_b_debug_print_sdf_top_down);
	ClassDB::bind_method(D_METHOD("debug_get_mesh_block_count"), &VoxelLodTerrain::_b_debug_get_mesh_block_count);
	ClassDB::bind_method(D_METHOD("debug_get_data_block_count"), &VoxelLodTerrain::_b_debug_get_data_block_count);
	ClassDB::bind_method(
			D_METHOD("debug_dump_as_scene", "path", "include_instancer"), &VoxelLodTerrain::_b_debug_dump_as_scene);
	ClassDB::bind_method(D_METHOD("debug_is_draw_enabled"), &VoxelLodTerrain::debug_is_draw_enabled);
	ClassDB::bind_method(D_METHOD("debug_set_draw_enabled", "enabled"), &VoxelLodTerrain::debug_set_draw_enabled);
	ClassDB::bind_method(
			D_METHOD("debug_set_draw_flag", "flag_index", "enabled"), &VoxelLodTerrain::debug_set_draw_flag);
	ClassDB::bind_method(D_METHOD("debug_get_draw_flag", "flag_index"), &VoxelLodTerrain::debug_get_draw_flag);

	// ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelLodTerrain::_on_stream_params_changed);

	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_IDLE);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_PHYSICS);
	BIND_ENUM_CONSTANT(PROCESS_CALLBACK_DISABLED);

	BIND_ENUM_CONSTANT(DEBUG_DRAW_OCTREE_NODES);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_OCTREE_BOUNDS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_MESH_UPDATES);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_EDIT_BOXES);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_VOLUME_BOUNDS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_EDITED_BLOCKS);
	BIND_ENUM_CONSTANT(DEBUG_DRAW_FLAGS_COUNT);

	ADD_GROUP("Bounds", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "voxel_bounds"), "set_voxel_bounds", "get_voxel_bounds");

	ADD_GROUP("Level of detail", "");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_distance"), "set_lod_distance", "get_lod_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod_fade_duration"), "set_lod_fade_duration", "get_lod_fade_duration");

	ADD_GROUP("Material", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE,
						 String("{0},{1}").format(
								 varray(BaseMaterial3D::get_class_static(), ShaderMaterial::get_class_static()))),
			"set_material", "get_material");

	ADD_GROUP("Detail normalmaps", "normalmap_");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalmap_enabled"), "set_normalmap_enabled", "is_normalmap_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "normalmap_tile_resolution_min"), "set_normalmap_tile_resolution_min",
			"get_normalmap_tile_resolution_min");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "normalmap_tile_resolution_max"), "set_normalmap_tile_resolution_max",
			"get_normalmap_tile_resolution_max");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "normalmap_begin_lod_index"), "set_normalmap_begin_lod_index",
			"get_normalmap_begin_lod_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "normalmap_max_deviation_degrees"), "set_normalmap_max_deviation_degrees",
			"get_normalmap_max_deviation_degrees");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalmap_octahedral_encoding_enabled"), "set_octahedral_normal_encoding",
			"get_octahedral_normal_encoding");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "normalmap_use_gpu"), "set_normalmap_use_gpu", "get_normalmap_use_gpu");

	ADD_GROUP("Collisions", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "generate_collisions"), "set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer",
			"get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask",
			"get_collision_mask");
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "collision_lod_count"), "set_collision_lod_count", "get_collision_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_update_delay"), "set_collision_update_delay",
			"get_collision_update_delay");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_margin"), "set_collision_margin", "get_collision_margin");

	ADD_GROUP("Advanced", "");

	// TODO Probably should be in parent class?
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"), "set_run_stream_in_editor",
			"is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_block_size"), "set_mesh_block_size", "get_mesh_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "full_load_mode_enabled"), "set_full_load_mode_enabled",
			"is_full_load_mode_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded_update_enabled"), "set_threaded_update_enabled",
			"is_threaded_update_enabled");
}

} // namespace zylann::voxel
