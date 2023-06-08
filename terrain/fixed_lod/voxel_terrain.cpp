#include "voxel_terrain.h"
#include "../../constants/voxel_constants.h"
#include "../../constants/voxel_string_names.h"
#include "../../edition/voxel_tool_terrain.h"
#include "../../engine/generate_block_task.h"
#include "../../engine/load_block_data_task.h"
#include "../../engine/mesh_block_task.h"
#include "../../engine/save_block_data_task.h"
#include "../../engine/voxel_engine.h"
#include "../../engine/voxel_engine_updater.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../storage/voxel_data.h"
#include "../../util/container_funcs.h"
#include "../../util/godot/classes/base_material_3d.h" // For property hint in release mode in GDExtension...
#include "../../util/godot/classes/concave_polygon_shape_3d.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/godot/classes/multiplayer_api.h"
#include "../../util/godot/classes/multiplayer_peer.h"
#include "../../util/godot/classes/scene_tree.h"
#include "../../util/godot/classes/script.h"
#include "../../util/godot/classes/shader_material.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/string.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/async_dependency_tracker.h"
#include "../instancing/voxel_instancer.h"
#include "../voxel_data_block_enter_info.h"
#include "../voxel_save_completion_tracker.h"
#include "voxel_terrain_multiplayer_synchronizer.h"

#ifdef TOOLS_ENABLED
#include "../../meshers/transvoxel/voxel_mesher_transvoxel.h"
#endif

namespace zylann::voxel {

VoxelTerrain::VoxelTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	set_notify_transform(true);

	_data = make_shared_instance<VoxelData>();

	// TODO Should it actually be finite for better discovery?
	// Infinite by default
	_data->set_bounds(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));

	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_meshing_dependency = make_shared_instance<MeshingDependency>();

	struct ApplyMeshUpdateTask : public ITimeSpreadTask {
		void run(TimeSpreadTaskContext &ctx) override {
			if (!VoxelEngine::get_singleton().is_volume_valid(volume_id)) {
				// The node can have been destroyed while this task was still pending
				ZN_PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
				return;
			}
			self->apply_mesh_update(data);
		}
		VolumeID volume_id;
		VoxelTerrain *self = nullptr;
		VoxelEngine::BlockMeshOutput data;
	};

	// Mesh updates are spread over frames by scheduling them in a task runner of VoxelEngine,
	// but instead of using a reception buffer we use a callback,
	// because this kind of task scheduling would otherwise delay the update by 1 frame
	VoxelEngine::VolumeCallbacks callbacks;
	callbacks.data = this;
	callbacks.mesh_output_callback = [](void *cb_data, VoxelEngine::BlockMeshOutput &ob) {
		VoxelTerrain *self = reinterpret_cast<VoxelTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->_volume_id;
		task->self = self;
		task->data = std::move(ob);
		VoxelEngine::get_singleton().push_main_thread_time_spread_task(task);
	};
	callbacks.data_output_callback = [](void *cb_data, VoxelEngine::BlockDataOutput &ob) {
		VoxelTerrain *self = reinterpret_cast<VoxelTerrain *>(cb_data);
		self->apply_data_block_response(ob);
	};

	_volume_id = VoxelEngine::get_singleton().add_volume(callbacks);

	// TODO Can't setup a default mesher anymore due to a Godot 4 warning...
	// For ease of use in editor
	// Ref<VoxelMesherBlocky> default_mesher;
	// default_mesher.instantiate();
	// _mesher = default_mesher;
}

VoxelTerrain::~VoxelTerrain() {
	ZN_PRINT_VERBOSE("Destroying VoxelTerrain");
	_streaming_dependency->valid = false;
	_meshing_dependency->valid = false;
	VoxelEngine::get_singleton().remove_volume(_volume_id);
}

void VoxelTerrain::set_material_override(Ref<Material> material) {
	if (_material_override == material) {
		return;
	}
	_material_override = material;
	_mesh_map.for_each_block([material](VoxelMeshBlockVT &block) { //
		block.set_material_override(material);
	});
}

Ref<Material> VoxelTerrain::get_material_override() const {
	return _material_override;
}

void VoxelTerrain::set_stream(Ref<VoxelStream> p_stream) {
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
				_run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelStream> VoxelTerrain::get_stream() const {
	return _data->get_stream();
}

void VoxelTerrain::set_generator(Ref<VoxelGenerator> p_generator) {
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
				_run_stream_in_editor = false;
				notify_property_list_changed();
			}
		}
	}
#endif

	_on_stream_params_changed();
}

Ref<VoxelGenerator> VoxelTerrain::get_generator() const {
	return _data->get_generator();
}

// void VoxelTerrain::_set_block_size_po2(int p_block_size_po2) {
// 	_data_map.create(0);
// }

unsigned int VoxelTerrain::get_data_block_size_pow2() const {
	return _data->get_block_size_po2();
}

unsigned int VoxelTerrain::get_mesh_block_size_pow2() const {
	return _mesh_block_size_po2;
}

void VoxelTerrain::set_mesh_block_size(unsigned int mesh_block_size) {
	mesh_block_size = math::clamp(mesh_block_size, get_data_block_size(), constants::MAX_BLOCK_SIZE);

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

	_mesh_block_size_po2 = po2;
        


	if (_instancer != nullptr) {
		VoxelInstancer &instancer = *_instancer;
		_mesh_map.for_each_block([&instancer, this](VoxelMeshBlockVT &block) { //
			instancer.on_mesh_block_exit(block.position, 0);
                        emit_mesh_block_exited(block.position);
		});
	} else {
                _mesh_map.for_each_block([this](VoxelMeshBlockVT &block) { //
                        emit_mesh_block_exited(block.position);
                });
        }

	// Unload all mesh blocks regardless of refcount
	_mesh_map.clear();

	// Make paired viewers re-view the new meshable area
	for (unsigned int i = 0; i < _paired_viewers.size(); ++i) {
		PairedViewer &viewer = _paired_viewers[i];
		// Resetting both because it's a re-initialization.
		// We could also be doing that before or after their are shifted.
		viewer.state.mesh_box = Box3i();
		viewer.prev_state.mesh_box = Box3i();
	}

	// VoxelEngine::get_singleton().set_volume_render_block_size(_volume_id, mesh_block_size);

	// No update on bounds because we can support a mismatch, as long as it is a multiple of data block size
	// set_bounds(_bounds_in_voxels);
}

void VoxelTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	// if (_stream.is_valid()) {
	// 	const int stream_block_size_po2 = _stream->get_block_size_po2();
	// 	_set_block_size_po2(stream_block_size_po2);
	// }

	// The whole map might change, so regenerate it
	reset_map();

	if ((get_stream().is_valid() || get_generator().is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	update_configuration_warnings();
}

void VoxelTerrain::_on_gi_mode_changed() {
	const GIMode gi_mode = get_gi_mode();
	_mesh_map.for_each_block([gi_mode](VoxelMeshBlockVT &block) { //
		block.set_gi_mode(DirectMeshInstance::GIMode(gi_mode));
	});
}

void VoxelTerrain::_on_shadow_casting_changed() {
	const RenderingServer::ShadowCastingSetting mode = RenderingServer::ShadowCastingSetting(get_shadow_casting());
	_mesh_map.for_each_block([mode](VoxelMeshBlockVT &block) { //
		block.set_shadow_casting(mode);
	});
}

Ref<VoxelMesher> VoxelTerrain::get_mesher() const {
	return _mesher;
}

void VoxelTerrain::set_mesher(Ref<VoxelMesher> mesher) {
	if (mesher == _mesher) {
		return;
	}

	_mesher = mesher;

	MeshingDependency::reset(_meshing_dependency, _mesher, get_generator());

	stop_updater();

	if (_mesher.is_valid()) {
		start_updater();
		// Voxel appearance might completely change
		remesh_all_blocks();
	}

	update_configuration_warnings();
}

void VoxelTerrain::get_viewers_in_area(std::vector<ViewerID> &out_viewer_ids, Box3i voxel_box) const {
	const Box3i block_box = voxel_box.downscaled(get_data_block_size());

	for (auto it = _paired_viewers.begin(); it != _paired_viewers.end(); ++it) {
		const PairedViewer &viewer = *it;

		if (viewer.state.data_box.intersects(block_box)) {
			out_viewer_ids.push_back(viewer.id);
		}
	}
}

void VoxelTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelTerrain::set_collision_layer(int layer) {
	_collision_layer = layer;
	_mesh_map.for_each_block([layer](VoxelMeshBlockVT &block) { //
		block.set_collision_layer(layer);
	});
}

int VoxelTerrain::get_collision_layer() const {
	return _collision_layer;
}

void VoxelTerrain::set_collision_mask(int mask) {
	_collision_mask = mask;
	_mesh_map.for_each_block([mask](VoxelMeshBlockVT &block) { //
		block.set_collision_mask(mask);
	});
}

int VoxelTerrain::get_collision_mask() const {
	return _collision_mask;
}

void VoxelTerrain::set_collision_margin(float margin) {
	_collision_margin = margin;
	_mesh_map.for_each_block([margin](VoxelMeshBlockVT &block) { //
		block.set_collision_margin(margin);
	});
}

float VoxelTerrain::get_collision_margin() const {
	return _collision_margin;
}

unsigned int VoxelTerrain::get_max_view_distance() const {
	return _max_view_distance_voxels;
}

void VoxelTerrain::set_max_view_distance(unsigned int distance_in_voxels) {
	ERR_FAIL_COND(distance_in_voxels < 0);
	_max_view_distance_voxels = distance_in_voxels;

	if (_instancer != nullptr) {
		_instancer->set_mesh_lod_distance(_max_view_distance_voxels);
	}
}

void VoxelTerrain::set_block_enter_notification_enabled(bool enable) {
	_block_enter_notification_enabled = enable;

	if (enable == false) {
		for (auto it = _loading_blocks.begin(); it != _loading_blocks.end(); ++it) {
			LoadingBlock &lb = it->second;
			lb.viewers_to_notify.clear();
		}
	}
}

bool VoxelTerrain::is_block_enter_notification_enabled() const {
	return _block_enter_notification_enabled;
}

void VoxelTerrain::set_area_edit_notification_enabled(bool enable) {
	_area_edit_notification_enabled = enable;
}

bool VoxelTerrain::is_area_edit_notification_enabled() const {
	return _area_edit_notification_enabled;
}

void VoxelTerrain::set_automatic_loading_enabled(bool enable) {
	_automatic_loading_enabled = enable;
}

bool VoxelTerrain::is_automatic_loading_enabled() const {
	return _automatic_loading_enabled;
}

void VoxelTerrain::try_schedule_mesh_update(VoxelMeshBlockVT &mesh_block) {
	if (mesh_block.get_mesh_state() == VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT) {
		// Already in the list
		return;
	}
	if (mesh_block.mesh_viewers.get() == 0 && mesh_block.collision_viewers.get() == 0) {
		// No viewers want mesh on this block (why even call this function then?)
		return;
	}

	const int render_to_data_factor = get_mesh_block_size() / get_data_block_size();

	const Box3i data_box =
			Box3i(mesh_block.position * render_to_data_factor, Vector3iUtil::create(render_to_data_factor)).padded(1);

	// If we get an empty box at this point, something is wrong with the caller
	ZN_ASSERT_RETURN(!data_box.is_empty());

	const bool data_available = _data->has_all_blocks_in_area(data_box);

	if (data_available) {
		// Regardless of if the updater is updating the block already,
		// the block could have been modified again so we schedule another update
		mesh_block.set_mesh_state(VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT);
		_blocks_pending_update.push_back(mesh_block.position);
	}
}

void VoxelTerrain::view_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag) {
	if (mesh_flag == false && collision_flag == false) {
		// Why even call the function?
		return;
	}

	VoxelMeshBlockVT *block = _mesh_map.get_block(bpos);

	if (block == nullptr) {
		// Create if not found
		block = memnew(VoxelMeshBlockVT(bpos, get_mesh_block_size()));
		block->set_world(get_world_3d());
		_mesh_map.set_block(bpos, block);
	}
	CRASH_COND(block == nullptr);

	if (mesh_flag) {
		block->mesh_viewers.add();
	}
	if (collision_flag) {
		block->collision_viewers.add();
	}

	// This is needed in case a viewer wants to view meshes in places data blocks are already present.
	// Before that, meshes were updated only when a data block was loaded or modified,
	// so changing block size or viewer flags did not make meshes appear.
	try_schedule_mesh_update(*block);

	// TODO viewers with varying flags during the game is not supported at the moment.
	// They have to be re-created, which may cause world re-load...
}

void VoxelTerrain::unview_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag) {
	VoxelMeshBlockVT *block = _mesh_map.get_block(bpos);
	// Mesh blocks are created on first view call,
	// so that would mean we unview one without viewing it in the first place
	ERR_FAIL_COND(block == nullptr);

	if (mesh_flag) {
		block->mesh_viewers.remove();
		if (block->mesh_viewers.get() == 0) {
			// Mesh no longer required
			block->drop_mesh();
		}
	}

	if (collision_flag) {
		block->collision_viewers.remove();
		if (block->collision_viewers.get() == 0) {
			// Collision no longer required
			block->drop_collision();
		}
	}

	if (block->mesh_viewers.get() == 0 && block->collision_viewers.get() == 0) {
		unload_mesh_block(bpos);
	}
}

void VoxelTerrain::unload_mesh_block(Vector3i bpos) {
	std::vector<Vector3i> &blocks_pending_update = _blocks_pending_update;

	_mesh_map.remove_block(bpos, [&blocks_pending_update](const VoxelMeshBlockVT &block) {
		if (block.get_mesh_state() == VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT) {
			// That block was in the list of blocks to update later in the process loop, we'll need to unregister
			// it. We expect that block to be in that list. If it isn't, something wrong happened with its state.
			ERR_FAIL_COND(!unordered_remove_value(blocks_pending_update, block.position));
		}
	});

	if (_instancer != nullptr) {
		_instancer->on_mesh_block_exit(bpos, 0);
	}
        emit_mesh_block_exited(bpos);
}

void VoxelTerrain::save_all_modified_blocks(bool with_copy, std::shared_ptr<AsyncDependencyTracker> tracker) {
	ZN_PROFILE_SCOPE();
	Ref<VoxelStream> stream = get_stream();
	ERR_FAIL_COND_MSG(stream.is_null(), "Attempting to save modified blocks, but there is no stream to save them to.");

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	// That may cause a stutter, so should be used when the player won't notice
	_data->consume_all_modifications(_blocks_to_save, with_copy);

	if (stream.is_valid() && _instancer != nullptr && stream->supports_instance_blocks()) {
		_instancer->save_all_modified_blocks(task_scheduler, tracker);
	}

	// And flush immediately
	consume_block_data_save_requests(task_scheduler, tracker);
	task_scheduler.flush();
}

const VoxelTerrain::Stats &VoxelTerrain::get_stats() const {
	return _stats;
}

void VoxelTerrain::set_instancer(VoxelInstancer *instancer) {
	if (_instancer != nullptr && instancer != nullptr) {
		ERR_FAIL_COND_MSG(_instancer != nullptr, "No more than one VoxelInstancer per terrain");
	}
	_instancer = instancer;
}

void VoxelTerrain::get_meshed_block_positions(std::vector<Vector3i> &out_positions) const {
	_mesh_map.for_each_block([&out_positions](const VoxelMeshBlock &mesh_block) {
		if (mesh_block.has_mesh()) {
			out_positions.push_back(mesh_block.position);
		}
	});
}

// This function is primarily intended for editor use cases at the moment.
// It will be slower than using the instancing generation events,
// because it has to query VisualServer, which then allocates and decodes vertex buffers (assuming they are cached).
Array VoxelTerrain::get_mesh_block_surface(Vector3i block_pos) const {
	ZN_PROFILE_SCOPE();

	Ref<Mesh> mesh;
	{
		const VoxelMeshBlockVT *block = _mesh_map.get_block(block_pos);
		if (block != nullptr) {
			mesh = block->get_mesh();
		}
	}

	if (mesh.is_valid()) {
		return mesh->surface_get_arrays(0);
	}

	return Array();
}

Dictionary VoxelTerrain::_b_get_statistics() const {
	Dictionary d;

	// Breakdown of time spent in _process
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_request_blocks_to_load"] = _stats.time_request_blocks_to_load;
	d["time_process_load_responses"] = _stats.time_process_load_responses;
	d["time_request_blocks_to_update"] = _stats.time_request_blocks_to_update;

	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;
	d["updated_blocks"] = _stats.updated_blocks;

	return d;
}

void VoxelTerrain::start_updater() {
	Ref<VoxelMesherBlocky> blocky_mesher = _mesher;
	if (blocky_mesher.is_valid()) {
		Ref<VoxelBlockyLibrary> library = blocky_mesher->get_library();
		if (library.is_valid()) {
			// TODO Any way to execute this function just after the TRES resource loader has finished to load?
			// VoxelBlockyLibrary should be baked ahead of time, like MeshLibrary
			library->bake();
		}
	}

	// VoxelEngine::get_singleton().set_volume_mesher(_volume_id, _mesher);
}

void VoxelTerrain::stop_updater() {
	// Invalidate pending tasks
	MeshingDependency::reset(_meshing_dependency, _mesher, get_generator());

	// VoxelEngine::get_singleton().set_volume_mesher(_volume_id, Ref<VoxelMesher>());

	// TODO We can still receive a few mesh delayed mesh updates after this. Is it a problem?
	//_reception_buffers.mesh_output.clear();

	_blocks_pending_update.clear();

	_mesh_map.for_each_block([](VoxelMeshBlockVT &block) {
		if (block.get_mesh_state() == VoxelMeshBlockVT::MESH_UPDATE_SENT) {
			block.set_mesh_state(VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT);
		}
	});
}

void VoxelTerrain::remesh_all_blocks() {
	_mesh_map.for_each_block([this](VoxelMeshBlockVT &block) { //
		try_schedule_mesh_update(block);
	});
}

// At the moment, this function is for client-side use case in multiplayer scenarios
void VoxelTerrain::generate_block_async(Vector3i block_position) {
	if (_data->has_block(block_position, 0)) {
		// Already exists
		return;
	}
	if (_loading_blocks.find(block_position) != _loading_blocks.end()) {
		// Already loading
		return;
	}

	// if (require_notification) {
	// 	new_loading_block.viewers_to_notify.push_back(viewer_id);
	// }

	LoadingBlock new_loading_block;
	const Box3i block_box(_data->block_to_voxel(block_position), Vector3iUtil::create(_data->get_block_size()));
	for (size_t i = 0; i < _paired_viewers.size(); ++i) {
		const PairedViewer &viewer = _paired_viewers[i];
		if (viewer.state.data_box.intersects(block_box)) {
			new_loading_block.viewers.add();
		}
	}

	if (new_loading_block.viewers.get() == 0) {
		return;
	}

	// Schedule a loading request
	// TODO This could also end up loading from stream
	_loading_blocks.insert({ block_position, new_loading_block });
	_blocks_pending_load.push_back(block_position);
}

void VoxelTerrain::start_streamer() {
	// VoxelEngine::get_singleton().set_volume_stream(_volume_id, _stream);
	// VoxelEngine::get_singleton().set_volume_generator(_volume_id, _generator);
}

void VoxelTerrain::stop_streamer() {
	// Invalidate pending tasks
	StreamingDependency::reset(_streaming_dependency, get_stream(), get_generator());
	// VoxelEngine::get_singleton().set_volume_stream(_volume_id, Ref<VoxelStream>());
	// VoxelEngine::get_singleton().set_volume_generator(_volume_id, Ref<VoxelGenerator>());
	_loading_blocks.clear();
	_blocks_pending_load.clear();
}

void VoxelTerrain::reset_map() {
	// Discard everything, to reload it all

	_data->for_each_block([this](const Vector3i &bpos, const VoxelDataBlock &block) { //
		emit_data_block_unloaded(bpos);
	});
	_data->reset_maps();

	_mesh_map.clear();

	_loading_blocks.clear();
	_blocks_pending_load.clear();
	_blocks_pending_update.clear();
	_blocks_to_save.clear();

	// No need to care about refcounts, we drop everything anyways. Will pair it back on next process.
	_paired_viewers.clear();
}

void VoxelTerrain::post_edit_voxel(Vector3i pos) {
	post_edit_area(Box3i(pos, Vector3i(1, 1, 1)));
}

void VoxelTerrain::try_schedule_mesh_update_from_data(const Box3i &box_in_voxels) {
	// We pad by 1 because neighbor blocks might be affected visually (for example, baked ambient occlusion)
	const Box3i mesh_box = box_in_voxels.padded(1).downscaled(get_mesh_block_size());
	mesh_box.for_each_cell([this](Vector3i pos) {
		VoxelMeshBlockVT *block = _mesh_map.get_block(pos);
		// There isn't necessarily a mesh block, if the edit happens in a boundary,
		// or if it is done next to a viewer that doesn't need meshes
		if (block != nullptr) {
			try_schedule_mesh_update(*block);
		}
	});
}

void VoxelTerrain::post_edit_area(Box3i box_in_voxels) {
	_data->mark_area_modified(box_in_voxels, nullptr);

	box_in_voxels.clip(_data->get_bounds());

	// TODO Maybe remove this in preference for multiplayer synchronizer virtual functions?
	if (_area_edit_notification_enabled) {
#if defined(ZN_GODOT)
		GDVIRTUAL_CALL(_on_area_edited, box_in_voxels.pos, box_in_voxels.size);
#else
		ERR_PRINT_ONCE("VoxelTerrain::_on_area_edited is not supported yet in GDExtension!");
#endif
	}

	if (_multiplayer_synchronizer != nullptr && _multiplayer_synchronizer->is_server()) {
		_multiplayer_synchronizer->send_area(box_in_voxels);
	}

	try_schedule_mesh_update_from_data(box_in_voxels);

	if (_instancer != nullptr) {
		_instancer->on_area_edited(box_in_voxels);
	}
}

void VoxelTerrain::_notification(int p_what) {
	struct SetWorldAction {
		World3D *world;
		SetWorldAction(World3D *w) : world(w) {}
		void operator()(VoxelMeshBlockVT &block) {
			block.set_world(world);
		}
	};

	struct SetParentVisibilityAction {
		bool visible;
		SetParentVisibilityAction(bool v) : visible(v) {}
		void operator()(VoxelMeshBlockVT &block) {
			block.set_parent_visible(visible);
		}
	};

	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			set_process(true);
#ifdef TOOLS_ENABLED
			// In the editor, auto-configure a default mesher, for convenience.
			// Because Godot has a property hint to automatically instantiate a resource, but if that resource is
			// abstract, it doesn't work... and it cannot be a default value because such practice was deprecated with a
			// warning in Godot 4.
			if (Engine::get_singleton()->is_editor_hint() && !get_mesher().is_valid()) {
				Ref<VoxelMesherTransvoxel> mesher;
				mesher.instantiate();
				set_mesher(mesher);
			}
#endif
			break;

		case NOTIFICATION_PROCESS:
			// Can't do that in enter tree because Godot is "still setting up children".
			// Can't do that in ready either because Godot says node state is locked.
			// This hack is quite miserable.
			VoxelEngineUpdater::ensure_existence(get_tree());

			process();
			break;

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD:
			_mesh_map.for_each_block(SetWorldAction(*get_world_3d()));
			break;

		case NOTIFICATION_EXIT_WORLD:
			_mesh_map.for_each_block(SetWorldAction(nullptr));
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			_mesh_map.for_each_block(SetParentVisibilityAction(is_visible()));
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			const Transform3D transform = get_global_transform();
			// VoxelEngine::get_singleton().set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			_mesh_map.for_each_block([&transform](VoxelMeshBlockVT &block) { //
				block.set_parent_transform(transform);
			});

		} break;

		default:
			break;
	}
}

inline Vector3i get_block_center(Vector3i pos, int bs) {
	return pos * bs + Vector3iUtil::create(bs / 2);
}

static void init_sparse_grid_priority_dependency(PriorityDependency &dep, Vector3i block_position, int block_size,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform) {
	const Vector3i voxel_pos = get_block_center(block_position, block_size);
	const float block_radius = block_size / 2;
	dep.shared = shared_viewers_data;
	dep.world_position = volume_transform.xform(voxel_pos);
	const float transformed_block_radius =
			volume_transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	// Distance beyond which no field of view can overlap the block.
	// Doubling block radius to account for an extra margin of blocks,
	// since they are used to provide neighbors when meshing
	dep.drop_distance_squared =
			math::squared(shared_viewers_data->highest_view_distance + 2.f * transformed_block_radius);
}

static void request_block_load(VolumeID volume_id, std::shared_ptr<StreamingDependency> stream_dependency,
		uint32_t data_block_size, Vector3i block_pos,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D volume_transform,
		bool request_instances) {
	ZN_ASSERT(stream_dependency != nullptr);

	if (stream_dependency->stream.is_valid()) {
		PriorityDependency priority_dependency;
		init_sparse_grid_priority_dependency(
				priority_dependency, block_pos, data_block_size, shared_viewers_data, volume_transform);

		LoadBlockDataTask *task = ZN_NEW(LoadBlockDataTask(volume_id, block_pos, 0, data_block_size, request_instances,
				stream_dependency, priority_dependency, true));

		VoxelEngine::get_singleton().push_async_io_task(task);

	} else {
		// Directly generate the block without checking the stream
		ERR_FAIL_COND(stream_dependency->generator.is_null());

		GenerateBlockTask *task = ZN_NEW(GenerateBlockTask);
		task->volume_id = volume_id;
		task->position = block_pos;
		task->lod = 0;
		task->block_size = data_block_size;
		task->stream_dependency = stream_dependency;

		init_sparse_grid_priority_dependency(
				task->priority_dependency, block_pos, data_block_size, shared_viewers_data, volume_transform);

		VoxelEngine::get_singleton().push_async_task(task);
	}
}

void VoxelTerrain::send_data_load_requests() {
	ZN_PROFILE_SCOPE();

	if (_blocks_pending_load.size() > 0) {
		std::shared_ptr<PriorityDependency::ViewersData> shared_viewers_data =
				VoxelEngine::get_singleton().get_shared_viewers_data_from_default_world();

		const Transform3D volume_transform = get_global_transform();

		// Blocks to load
		for (size_t i = 0; i < _blocks_pending_load.size(); ++i) {
			const Vector3i block_pos = _blocks_pending_load[i];
			// TODO Optimization: Batch request
			request_block_load(_volume_id, _streaming_dependency, get_data_block_size(), block_pos, shared_viewers_data,
					volume_transform, _instancer != nullptr);
		}
		_blocks_pending_load.clear();
	}
}

void VoxelTerrain::consume_block_data_save_requests(
		BufferedTaskScheduler &task_scheduler, std::shared_ptr<AsyncDependencyTracker> saving_tracker) {
	ZN_PROFILE_SCOPE();

	// Blocks to save
	if (get_stream().is_valid()) {
		const uint8_t data_block_size = get_data_block_size();
		for (const VoxelData::BlockToSave &b : _blocks_to_save) {
			ZN_PRINT_VERBOSE(format("Requesting save of block {}", b.position));

			SaveBlockDataTask *task = ZN_NEW(SaveBlockDataTask(
					_volume_id, b.position, 0, data_block_size, b.voxels, _streaming_dependency, saving_tracker));

			// No priority data, saving doesn't need sorting.
			task_scheduler.push_io_task(task);
		}
	} else {
		if (_blocks_to_save.size() > 0) {
			ZN_PRINT_VERBOSE(format("Not saving {} blocks because no stream is assigned", _blocks_to_save.size()));
		}
	}

	if (saving_tracker != nullptr) {
		// Using buffered count instead of `_blocks_to_save` because it can also contain tasks from VoxelInstancer
		saving_tracker->set_count(task_scheduler.get_io_count());
	}

	// print_line(String("Sending {0} block requests").format(varray(input.blocks_to_emerge.size())));
	_blocks_to_save.clear();
}

void VoxelTerrain::emit_data_block_loaded(Vector3i bpos) {
	// Not sure about exposing buffers directly... some stuff on them is useful to obtain directly,
	// but also it allows scripters to mess with voxels in a way they should not.
	// Example: modifying voxels without locking them first, while another thread may be reading them at the same
	// time. The same thing could happen the other way around (threaded task modifying voxels while you try to read
	// them). It isn't planned to expose VoxelBuffer locks because there are too many of them, it may likely shift
	// to another system in the future, and might even be changed to no longer inherit Reference. So unless this is
	// absolutely necessary, buffers aren't exposed. Workaround: use VoxelTool
	// const Variant vbuffer = block->voxels;
	// const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton().block_loaded, bpos);
}

void VoxelTerrain::emit_data_block_unloaded(Vector3i bpos) {
	emit_signal(VoxelStringNames::get_singleton().block_unloaded, bpos);
}

void VoxelTerrain::emit_mesh_block_entered(Vector3i bpos) {
	// Not sure about exposing buffers directly... some stuff on them is useful to obtain directly,
	// but also it allows scripters to mess with voxels in a way they should not.
	// Example: modifying voxels without locking them first, while another thread may be reading them at the same
	// time. The same thing could happen the other way around (threaded task modifying voxels while you try to read
	// them). It isn't planned to expose VoxelBuffer locks because there are too many of them, it may likely shift
	// to another system in the future, and might even be changed to no longer inherit Reference. So unless this is
	// absolutely necessary, buffers aren't exposed. Workaround: use VoxelTool
	// const Variant vbuffer = block->voxels;
	// const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton().mesh_block_entered, bpos);
}

void VoxelTerrain::emit_mesh_block_exited(Vector3i bpos) {
	emit_signal(VoxelStringNames::get_singleton().mesh_block_exited, bpos);
}

bool VoxelTerrain::try_get_paired_viewer_index(ViewerID id, size_t &out_i) const {
	for (size_t i = 0; i < _paired_viewers.size(); ++i) {
		const PairedViewer &p = _paired_viewers[i];
		if (p.id == id) {
			out_i = i;
			return true;
		}
	}
	return false;
}

// TODO It is unclear yet if this API will stay. I have a feeling it might consume a lot of CPU
void VoxelTerrain::notify_data_block_enter(const VoxelDataBlock &block, Vector3i bpos, ViewerID viewer_id) {
	if (!VoxelEngine::get_singleton().viewer_exists(viewer_id)) {
		// The viewer might have been removed between the moment we requested the block and the moment we finished
		// loading it
		return;
	}
	if (_data_block_enter_info_obj == nullptr) {
		_data_block_enter_info_obj = gd_make_unique<VoxelDataBlockEnterInfo>();
	}
	const int network_peer_id = VoxelEngine::get_singleton().get_viewer_network_peer_id(viewer_id);
	_data_block_enter_info_obj->network_peer_id = network_peer_id;
	_data_block_enter_info_obj->voxel_block = block;
	_data_block_enter_info_obj->block_position = bpos;

#if defined(ZN_GODOT)
	if (!GDVIRTUAL_CALL(_on_data_block_entered, _data_block_enter_info_obj.get()) &&
			_multiplayer_synchronizer == nullptr) {
		WARN_PRINT_ONCE("VoxelTerrain::_on_data_block_entered is unimplemented!");
	}
#else
	ERR_PRINT_ONCE("VoxelTerrain::_on_data_block_entered is not supported yet in GDExtension!");
#endif

	if (_multiplayer_synchronizer != nullptr && !Engine::get_singleton()->is_editor_hint() &&
			network_peer_id != MultiplayerPeer::TARGET_PEER_SERVER && _multiplayer_synchronizer->is_server()) {
		_multiplayer_synchronizer->send_block(network_peer_id, block, bpos);
	}
}

void VoxelTerrain::process() {
	ZN_PROFILE_SCOPE();
	process_viewers();
	// process_received_data_blocks();
	process_meshing();
}

void VoxelTerrain::process_viewers() {
	ProfilingClock profiling_clock;

	// Ordered by ascending index in paired viewers list
	std::vector<size_t> unpaired_viewer_indexes;

	// Update viewers
	{
		// Our node doesn't have bounds yet, so for now viewers are always paired.
		// TODO Update: the node has bounds now, need to change this

		// Destroyed viewers
		for (size_t i = 0; i < _paired_viewers.size(); ++i) {
			PairedViewer &p = _paired_viewers[i];
			if (!VoxelEngine::get_singleton().viewer_exists(p.id)) {
				ZN_PRINT_VERBOSE(format("Detected destroyed viewer {} in VoxelTerrain", p.id));
				// Interpret removal as nullified view distance so the same code handling loading of blocks
				// will be used to unload those viewed by this viewer.
				// We'll actually remove unpaired viewers in a second pass.
				p.state.view_distance_voxels = 0;
				// Also update boxes, they won't be updated since the viewer has been removed.
				// Assign prev state, otherwise in some cases resetting boxes would make them equal to prev state,
				// therefore causing no unload
				p.prev_state = p.state;
				p.state.data_box = Box3i();
				p.state.mesh_box = Box3i();
				unpaired_viewer_indexes.push_back(i);
			}
		}

		const Transform3D local_to_world_transform = get_global_transform();
		const Transform3D world_to_local_transform = local_to_world_transform.affine_inverse();

		// Note, this does not support non-uniform scaling
		// TODO There is probably a better way to do this
		const float view_distance_scale = world_to_local_transform.basis.xform(Vector3(1, 0, 0)).length();

		const Box3i bounds_in_voxels = _data->get_bounds();

		const Box3i bounds_in_data_blocks = bounds_in_voxels.downscaled(get_data_block_size());
		const Box3i bounds_in_mesh_blocks = bounds_in_voxels.downscaled(get_mesh_block_size());

		struct UpdatePairedViewer {
			VoxelTerrain &self;
			const Box3i bounds_in_data_blocks;
			const Box3i bounds_in_mesh_blocks;
			const Transform3D world_to_local_transform;
			const float view_distance_scale;

			inline void operator()(ViewerID viewer_id, const VoxelEngine::Viewer &viewer) {
				size_t paired_viewer_index;
				if (!self.try_get_paired_viewer_index(viewer_id, paired_viewer_index)) {
					// New viewer
					PairedViewer p;
					p.id = viewer_id;
					paired_viewer_index = self._paired_viewers.size();
					self._paired_viewers.push_back(p);
					ZN_PRINT_VERBOSE(format("Pairing viewer {} to VoxelTerrain", viewer_id));
				}

				PairedViewer &paired_viewer = self._paired_viewers[paired_viewer_index];
				paired_viewer.prev_state = paired_viewer.state;
				PairedViewer::State &state = paired_viewer.state;

				const unsigned int view_distance_voxels =
						static_cast<unsigned int>(static_cast<float>(viewer.view_distance) * view_distance_scale);
				const Vector3 local_position = world_to_local_transform.xform(viewer.world_position);

				state.view_distance_voxels = math::min(view_distance_voxels, self._max_view_distance_voxels);
				state.local_position_voxels = math::floor_to_int(local_position);
				state.requires_collisions = VoxelEngine::get_singleton().is_viewer_requiring_collisions(viewer_id);
				state.requires_meshes =
						VoxelEngine::get_singleton().is_viewer_requiring_visuals(viewer_id) && self._mesher.is_valid();

				// Update data and mesh view boxes

				const int data_block_size = self.get_data_block_size();
				const int mesh_block_size = self.get_mesh_block_size();

				int view_distance_data_blocks;
				Vector3i data_block_pos;

				if (state.requires_meshes || state.requires_collisions) {
					const int view_distance_mesh_blocks = math::ceildiv(state.view_distance_voxels, mesh_block_size);
					const int render_to_data_factor = (mesh_block_size / data_block_size);
					const Vector3i mesh_block_pos = math::floordiv(state.local_position_voxels, mesh_block_size);

					// Adding one block of padding because meshing requires neighbors
					view_distance_data_blocks = view_distance_mesh_blocks * render_to_data_factor + 1;

					data_block_pos = mesh_block_pos * render_to_data_factor;
					state.mesh_box =
							Box3i::from_center_extents(mesh_block_pos, Vector3iUtil::create(view_distance_mesh_blocks))
									.clipped(bounds_in_mesh_blocks);

				} else {
					view_distance_data_blocks = math::ceildiv(state.view_distance_voxels, data_block_size);

					data_block_pos = math::floordiv(state.local_position_voxels, data_block_size);
					state.mesh_box = Box3i();
				}

				state.data_box =
						Box3i::from_center_extents(data_block_pos, Vector3iUtil::create(view_distance_data_blocks))
								.clipped(bounds_in_data_blocks);
			}
		};

		// New viewers and updates. Removed viewers won't be iterated but are still paired until later.
		UpdatePairedViewer u{ *this, bounds_in_data_blocks, bounds_in_mesh_blocks, world_to_local_transform,
			view_distance_scale };
		VoxelEngine::get_singleton().for_each_viewer(u);
	}

	const bool can_load_blocks =
			((_automatic_loading_enabled &&
					 (_multiplayer_synchronizer == nullptr || _multiplayer_synchronizer->is_server())) &&
					(get_stream().is_valid() || get_generator().is_valid())) &&
			(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor);

	// Find out which blocks need to appear and which need to be unloaded
	{
		ZN_PROFILE_SCOPE();

		for (size_t i = 0; i < _paired_viewers.size(); ++i) {
			const PairedViewer &viewer = _paired_viewers[i];

			{
				const Box3i &new_data_box = viewer.state.data_box;
				const Box3i &prev_data_box = viewer.prev_state.data_box;

				if (prev_data_box != new_data_box) {
					process_viewer_data_box_change(viewer.id, prev_data_box, new_data_box, can_load_blocks);
				}
			}

			{
				const Box3i &new_mesh_box = viewer.state.mesh_box;
				const Box3i &prev_mesh_box = viewer.prev_state.mesh_box;

				if (prev_mesh_box != new_mesh_box) {
					ZN_PROFILE_SCOPE();

					// TODO Any reason to unview old blocks before viewing new blocks?
					// Because if a viewer is removed and another is added, it will reload the whole area even if their
					// box is the same.

					// Unview blocks that just fell out of range
					prev_mesh_box.difference(new_mesh_box, [this, &viewer](Box3i out_of_range_box) {
						out_of_range_box.for_each_cell([this, &viewer](Vector3i bpos) {
							unview_mesh_block(
									bpos, viewer.prev_state.requires_meshes, viewer.prev_state.requires_collisions);
						});
					});

					// View blocks that just entered the range
					new_mesh_box.difference(prev_mesh_box, [this, &viewer](Box3i box_to_load) {
						box_to_load.for_each_cell([this, &viewer](Vector3i bpos) {
							// Load or update block
							view_mesh_block(bpos, viewer.state.requires_meshes, viewer.state.requires_collisions);
						});
					});
				}

				// Blocks that remained within range of the viewer may need some changes too if viewer flags were
				// modified. This operates on a DISTINCT set of blocks than the one above.

				if (viewer.state.requires_collisions != viewer.prev_state.requires_collisions) {
					const Box3i box = new_mesh_box.clipped(prev_mesh_box);
					if (viewer.state.requires_collisions) {
						box.for_each_cell([this](Vector3i bpos) { //
							view_mesh_block(bpos, false, true);
						});

					} else {
						box.for_each_cell([this](Vector3i bpos) { //
							unview_mesh_block(bpos, false, true);
						});
					}
				}

				if (viewer.state.requires_meshes != viewer.prev_state.requires_meshes) {
					const Box3i box = new_mesh_box.clipped(prev_mesh_box);
					if (viewer.state.requires_meshes) {
						box.for_each_cell([this](Vector3i bpos) { //
							view_mesh_block(bpos, true, false);
						});

					} else {
						box.for_each_cell([this](Vector3i bpos) { //
							unview_mesh_block(bpos, true, false);
						});
					}
				}
			}
		}
	}

	_stats.time_detect_required_blocks = profiling_clock.restart();

	// We no longer need unpaired viewers.
	for (size_t i = 0; i < unpaired_viewer_indexes.size(); ++i) {
		// Iterating backward so indexes of paired viewers that need removal will not change because of the removal
		// itself
		const size_t vi = unpaired_viewer_indexes[unpaired_viewer_indexes.size() - i - 1];
		ZN_PRINT_VERBOSE(format("Unpairing viewer {} from VoxelTerrain", _paired_viewers[vi].id));
		_paired_viewers[vi] = _paired_viewers.back();
		_paired_viewers.pop_back();
	}

	// It's possible the user didn't set a stream yet, or it is turned off
	if (can_load_blocks) {
		send_data_load_requests();
		BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();
		consume_block_data_save_requests(task_scheduler, nullptr);
		task_scheduler.flush();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();
}

void VoxelTerrain::process_viewer_data_box_change(
		ViewerID viewer_id, Box3i prev_data_box, Box3i new_data_box, bool can_load_blocks) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(prev_data_box != new_data_box);

	static thread_local std::vector<Vector3i> tls_missing_blocks;
	static thread_local std::vector<Vector3i> tls_found_blocks_positions;

	// Unview blocks that just fell out of range
	//
	// TODO Any reason to unview old blocks before viewing new blocks?
	// Because if a viewer is removed and another is added, it will reload the whole area even if their box is the same.
	{
		const bool may_save =
				get_stream().is_valid() && (!Engine::get_singleton()->is_editor_hint() || _run_stream_in_editor);

		tls_missing_blocks.clear();
		tls_found_blocks_positions.clear();

		// Decrement refcounts from loaded blocks, and unload them
		prev_data_box.difference(new_data_box, [this, may_save](Box3i out_of_range_box) {
			// ZN_PRINT_VERBOSE(format("Unview data box {}", out_of_range_box));
			_data->unview_area(out_of_range_box, tls_missing_blocks, tls_found_blocks_positions,
					may_save ? &_blocks_to_save : nullptr);
		});

		// Remove loading blocks (those were loaded and had their refcount reach zero)
		for (const Vector3i bpos : tls_found_blocks_positions) {
			emit_data_block_unloaded(bpos);
			_loading_blocks.erase(bpos);
		}

		// Remove refcount from loading blocks, and cancel loading if it reaches zero
		for (const Vector3i bpos : tls_missing_blocks) {
			auto loading_block_it = _loading_blocks.find(bpos);
			if (loading_block_it == _loading_blocks.end()) {
				ZN_PRINT_VERBOSE("Request to unview a loading block that was never requested");
				// Not expected, but fine I guess
				return;
			}

			LoadingBlock &loading_block = loading_block_it->second;
			loading_block.viewers.remove();

			if (loading_block.viewers.get() == 0) {
				// No longer want to load it
				_loading_blocks.erase(loading_block_it);

				// TODO Do we really need that vector after all?
				for (size_t i = 0; i < _blocks_pending_load.size(); ++i) {
					if (_blocks_pending_load[i] == bpos) {
						_blocks_pending_load[i] = _blocks_pending_load.back();
						_blocks_pending_load.pop_back();
						break;
					}
				}
			}
		}
	}

	// View blocks coming into range
	if (can_load_blocks) {
		const bool require_notifications =
				(_block_enter_notification_enabled ||
						(_multiplayer_synchronizer != nullptr && _multiplayer_synchronizer->is_server())) &&
				VoxelEngine::get_singleton().viewer_exists(viewer_id) && // Could be a destroyed viewer
				VoxelEngine::get_singleton().is_viewer_requiring_data_block_notifications(viewer_id);

		static thread_local std::vector<VoxelDataBlock> tls_found_blocks;

		tls_missing_blocks.clear();
		tls_found_blocks.clear();
		tls_found_blocks_positions.clear();

		new_data_box.difference(prev_data_box, [this](Box3i box_to_load) {
			// ZN_PRINT_VERBOSE(format("View data box {}", box_to_load));
			_data->view_area(box_to_load, tls_missing_blocks, tls_found_blocks_positions, tls_found_blocks);
		});

		// Schedule loading of missing blocks
		for (const Vector3i missing_bpos : tls_missing_blocks) {
			auto loading_block_it = _loading_blocks.find(missing_bpos);

			if (loading_block_it == _loading_blocks.end()) {
				// First viewer to request it
				LoadingBlock new_loading_block;
				new_loading_block.viewers.add();

				if (require_notifications) {
					new_loading_block.viewers_to_notify.push_back(viewer_id);
				}

				// Schedule a loading request
				_loading_blocks.insert({ missing_bpos, new_loading_block });
				_blocks_pending_load.push_back(missing_bpos);

			} else {
				// More viewers
				LoadingBlock &loading_block = loading_block_it->second;
				loading_block.viewers.add();

				if (require_notifications) {
					loading_block.viewers_to_notify.push_back(viewer_id);
				}
			}
		}

		if (require_notifications) {
			// Notifications for blocks that were already loaded
			for (unsigned int i = 0; i < tls_found_blocks.size(); ++i) {
				const Vector3i bpos = tls_found_blocks_positions[i];
				const VoxelDataBlock &block = tls_found_blocks[i];
				notify_data_block_enter(block, bpos, viewer_id);
			}
		}

		// Make sure to clear this because it holds refcounted stuff. If we don't, it could crash on exit because the
		// voxel engine deinitializes its stuff before thread_locals get destroyed
		tls_found_blocks.clear();

		// TODO viewers with varying flags during the game is not supported at the moment.
		// They have to be re-created, which may cause world re-load...
	}
}

void VoxelTerrain::apply_data_block_response(VoxelEngine::BlockDataOutput &ob) {
	ZN_PROFILE_SCOPE();

	// print_line(String("Receiving {0} blocks").format(varray(output.emerged_blocks.size())));

	if (ob.type == VoxelEngine::BlockDataOutput::TYPE_SAVED) {
		if (ob.dropped) {
			ERR_PRINT(String("Could not save block {0}").format(varray(ob.position)));
		}
		return;
	}

	CRASH_COND(ob.type != VoxelEngine::BlockDataOutput::TYPE_LOADED &&
			ob.type != VoxelEngine::BlockDataOutput::TYPE_GENERATED);

	const Vector3i block_pos = ob.position;

	if (ob.dropped) {
		// That block was cancelled by the server, but we are still expecting it.
		// We'll have to request it again.
		ZN_PRINT_VERBOSE(format("Received a block loading drop while we were still expecting it: "
								"lod{} ({}, {}, {}), re-requesting it",
				ob.lod, ob.position.x, ob.position.y, ob.position.z));

		++_stats.dropped_block_loads;

		_blocks_pending_load.push_back(ob.position);
		return;
	}

	LoadingBlock loading_block;
	{
		auto loading_block_it = _loading_blocks.find(block_pos);

		if (loading_block_it == _loading_blocks.end()) {
			// That block was not requested or is no longer needed, drop it.
			++_stats.dropped_block_loads;
			return;
		}

		// Using move semantics because it can contain an allocated vector
		loading_block = std::move(loading_block_it->second);

		// Now we got the block. If we still have to drop it, the cause will be an error.
		_loading_blocks.erase(loading_block_it);
	}

	CRASH_COND(ob.voxels == nullptr);

	VoxelDataBlock block(ob.voxels, ob.lod);
	block.set_edited(ob.type == VoxelEngine::BlockDataOutput::TYPE_LOADED);
	// Viewers will be set only if the block doesn't already exist
	block.viewers = loading_block.viewers;

	if (block.has_voxels() && block.get_voxels_const().get_size() != Vector3iUtil::create(_data->get_block_size())) {
		// Voxel block size is incorrect, drop it
		ZN_PRINT_ERROR("Block is different from expected size");
		++_stats.dropped_block_loads;
		return;
	}

	_data->try_set_block(block_pos, block, [](VoxelDataBlock &existing_block, const VoxelDataBlock &incoming_block) {
		existing_block.set_voxels(incoming_block.get_voxels_shared());
		existing_block.set_edited(incoming_block.is_edited());
	});

	emit_data_block_loaded(block_pos);

	for (unsigned int i = 0; i < loading_block.viewers_to_notify.size(); ++i) {
		const ViewerID viewer_id = loading_block.viewers_to_notify[i];
		notify_data_block_enter(block, block_pos, viewer_id);
	}

	// The block itself might not be suitable for meshing yet, but blocks surrounding it might be now
	{
		ZN_PROFILE_SCOPE();
		// TODO Optimize: initial loading can hang for a while here.
		// Because lots of blocks are loaded at once, which leads to many block queries.
		try_schedule_mesh_update_from_data(
				Box3i(_data->block_to_voxel(block_pos), Vector3iUtil::create(get_data_block_size())));
	}

	// We might have requested some blocks again (if we got a dropped one while we still need them)
	// if (stream_enabled) {
	// 	send_block_data_requests();
	// }

	if (_instancer != nullptr && ob.instances != nullptr) {
		_instancer->on_data_block_loaded(ob.position, ob.lod, std::move(ob.instances));
	}
}

// Sets voxel data of a block, discarding existing data if any.
// If the given block coordinates are not inside any viewer's area, this function won't do anything and return
// false. If a block is already loading or generating at this position, it will be cancelled.
bool VoxelTerrain::try_set_block_data(Vector3i position, std::shared_ptr<VoxelBufferInternal> &voxel_data) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND_V(voxel_data == nullptr, false);

	const Vector3i expected_block_size = Vector3iUtil::create(_data->get_block_size());
	ERR_FAIL_COND_V_MSG(voxel_data->get_size() != expected_block_size, false,
			String("Block size is different from expected size. "
				   "Expected {0}, got {1}")
					.format(varray(expected_block_size, voxel_data->get_size())));

	// Setup viewers count intersecting with this block
	RefCount refcount;
	for (unsigned int i = 0; i < _paired_viewers.size(); ++i) {
		const PairedViewer &viewer = _paired_viewers[i];
		if (viewer.state.data_box.contains(position)) {
			refcount.add();
		}
	}

	if (refcount.get() == 0) {
		// Actually, this block is not even in range. So we may ignore it.
		// If we don't want this behavior, we could introduce a fake viewer that adds a reference to all blocks in
		// this volume as long as it is enabled?
		ZN_PRINT_VERBOSE("Trying to set a data block outside of any viewer range");
		return false;
	}

	// Cancel loading version if any
	_loading_blocks.erase(position);

	VoxelDataBlock block(voxel_data, 0);
	// TODO How to set the `edited` flag? Does it matter in use cases for this function?
	block.set_edited(true);
	block.viewers = refcount;

	// Create or update block data
	_data->try_set_block(position, block, [](VoxelDataBlock &existing_block, const VoxelDataBlock &incoming_block) {
		existing_block.set_voxels(incoming_block.get_voxels_shared());
		existing_block.set_edited(incoming_block.is_edited());
	});

	// The block itself might not be suitable for meshing yet, but blocks surrounding it might be now
	try_schedule_mesh_update_from_data(
			Box3i(_data->block_to_voxel(position), Vector3iUtil::create(get_data_block_size())));

	return true;
}

bool VoxelTerrain::has_data_block(Vector3i position) const {
	return _data->has_block(position, 0);
}

void VoxelTerrain::process_meshing() {
	ZN_PROFILE_SCOPE();
	ProfilingClock profiling_clock;

	_stats.dropped_block_meshs = 0;

	// Send mesh updates

	const Transform3D volume_transform = get_global_transform();
	std::shared_ptr<PriorityDependency::ViewersData> shared_viewers_data =
			VoxelEngine::get_singleton().get_shared_viewers_data_from_default_world();

	// const int used_channels_mask = get_used_channels_mask();
	const int mesh_to_data_factor = get_mesh_block_size() / get_data_block_size();

	for (size_t bi = 0; bi < _blocks_pending_update.size(); ++bi) {
		ZN_PROFILE_SCOPE();
		const Vector3i mesh_block_pos = _blocks_pending_update[bi];

		VoxelMeshBlockVT *mesh_block = _mesh_map.get_block(mesh_block_pos);

		// If we got here, it must have been because of scheduling an update
		ERR_CONTINUE(mesh_block == nullptr);
		ERR_CONTINUE(mesh_block->get_mesh_state() != VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT);

		// Pad by 1 because meshing requires neighbors
		const Box3i data_box =
				Box3i(mesh_block_pos * mesh_to_data_factor, Vector3iUtil::create(mesh_to_data_factor)).padded(1);

#ifdef DEBUG_ENABLED
		// We must have picked up a valid data block
		{
			const Vector3i anchor_pos = data_box.pos + Vector3i(1, 1, 1);
			ERR_CONTINUE(!_data->has_block(anchor_pos, 0));
		}
#endif

		// print_line(String("DDD request {0}").format(varray(mesh_request.render_block_position.to_vec3())));
		// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
		MeshBlockTask *task = ZN_NEW(MeshBlockTask);
		task->volume_id = _volume_id;
		task->mesh_block_position = mesh_block_pos;
		task->lod_index = 0;
		task->meshing_dependency = _meshing_dependency;
		task->data_block_size = get_data_block_size();
		task->collision_hint = _generate_collisions;

		// This iteration order is specifically chosen to match VoxelEngine and threaded access
		_data->get_blocks_with_voxel_data(data_box, 0, to_span(task->blocks));
		task->blocks_count = Vector3iUtil::get_volume(data_box.size);

#ifdef DEBUG_ENABLED
		{
			unsigned int count = 0;
			for (unsigned int i = 0; i < task->blocks_count; ++i) {
				if (task->blocks[i] != nullptr) {
					++count;
				}
			}
			// Blocks that were in the list must have been scheduled because we have data for them!
			if (count == 0) {
				ZN_PRINT_ERROR("Unexpected empty block list in meshing block task");
				ZN_DELETE(task);
				continue;
			}
		}
#endif

		init_sparse_grid_priority_dependency(task->priority_dependency, task->mesh_block_position,
				get_mesh_block_size(), shared_viewers_data, volume_transform);

		VoxelEngine::get_singleton().push_async_task(task);

		mesh_block->set_mesh_state(VoxelMeshBlockVT::MESH_UPDATE_SENT);
	}

	_blocks_pending_update.clear();

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") +
	// String::num(_block_update_queue.size()));
}

void VoxelTerrain::apply_mesh_update(const VoxelEngine::BlockMeshOutput &ob) {
	ZN_PROFILE_SCOPE();
	// print_line(String("DDD receive {0}").format(varray(ob.position.to_vec3())));

	VoxelMeshBlockVT *block = _mesh_map.get_block(ob.position);
	if (block == nullptr) {
		// print_line("- no longer loaded");
		// That block is no longer loaded, drop the result
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

	// There is a slim chance for some updates to come up just after setting the mesher to null. Avoids a crash.
	if (_mesher.is_null()) {
		++_stats.dropped_block_meshs;
		return;
	}

	Ref<ArrayMesh> mesh;
	std::vector<uint8_t> material_indices;
	if (ob.has_mesh_resource) {
		// The mesh was already built as part of the threaded task
		mesh = ob.mesh;
		// It can be empty
		material_indices = std::move(ob.mesh_material_indices);
	} else {
		// Can't build meshes in threads, do it here
		material_indices.clear();
		mesh = build_mesh(to_span_const(ob.surfaces.surfaces), ob.surfaces.primitive_type, ob.surfaces.mesh_flags,
				material_indices);
	}
	if (mesh.is_valid()) {
		const unsigned int surface_count = mesh->get_surface_count();
		for (unsigned int surface_index = 0; surface_index < surface_count; ++surface_index) {
			const unsigned int material_index = material_indices[surface_index];
			Ref<Material> material = _mesher->get_material_by_index(material_index);
			mesh->surface_set_material(surface_index, material);
		}
	}

	
        if (mesh.is_null() && block != nullptr) {
                // No surface anymore in this block
                if (_instancer != nullptr) {
                        _instancer->on_mesh_block_exit(ob.position, ob.lod);
                }
                emit_mesh_block_exited(ob.position);
        }
        if (ob.surfaces.surfaces.size() > 0 && mesh.is_valid() && !block->has_mesh()) {
                // TODO The mesh could come from an edited region!
                // We would have to know if specific voxels got edited, or different from the generator
                // TODO Support multi-surfaces in VoxelInstancer
                if (_instancer != nullptr) {
                        _instancer->on_mesh_block_enter(ob.position, ob.lod, ob.surfaces.surfaces[0].arrays);
                }
                emit_mesh_block_entered(ob.position);
        }

	block->set_mesh(mesh, DirectMeshInstance::GIMode(get_gi_mode()),
			RenderingServer::ShadowCastingSetting(get_shadow_casting()));

	if (_material_override.is_valid()) {
		block->set_material_override(_material_override);
	}

	const bool gen_collisions = _generate_collisions && block->collision_viewers.get() > 0;
	if (gen_collisions) {
		Ref<Shape3D> collision_shape = make_collision_shape_from_mesher_output(ob.surfaces, **_mesher);
		const bool debug_collisions = is_inside_tree() ? get_tree()->is_debugging_collisions_hint() : false;
		block->set_collision_shape(collision_shape, debug_collisions, this, _collision_margin);

		block->set_collision_layer(_collision_layer);
		block->set_collision_mask(_collision_mask);
	}

	block->set_visible(true);
	block->set_parent_visible(is_visible());
	block->set_parent_transform(get_global_transform());
	// TODO We don't set MESH_UP_TO_DATE anywhere, but it seems to work?
}

Ref<VoxelTool> VoxelTerrain::get_voxel_tool() {
	Ref<VoxelTool> vt = memnew(VoxelToolTerrain(this));
	const int used_channels_mask = get_used_channels_mask();
	// Auto-pick first used channel
	for (int channel = 0; channel < VoxelBufferInternal::MAX_CHANNELS; ++channel) {
		if ((used_channels_mask & (1 << channel)) != 0) {
			vt->set_channel(VoxelBufferInternal::ChannelId(channel));
			break;
		}
	}
	return vt;
}

void VoxelTerrain::set_run_stream_in_editor(bool enable) {
	if (enable == _run_stream_in_editor) {
		return;
	}

	_run_stream_in_editor = enable;

	if (Engine::get_singleton()->is_editor_hint()) {
		if (_run_stream_in_editor) {
			_on_stream_params_changed();

		} else {
			// This is expected to block the main thread until the streaming thread is done.
			stop_streamer();
		}
	}
}

bool VoxelTerrain::is_stream_running_in_editor() const {
	return _run_stream_in_editor;
}

void VoxelTerrain::set_bounds(Box3i box) {
	Box3i bounds_in_voxels =
			box.clipped(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));

	// Round to block size
	bounds_in_voxels = bounds_in_voxels.snapped(get_data_block_size());

	_data->set_bounds(bounds_in_voxels);

	const unsigned int largest_dimension =
			static_cast<unsigned int>(math::max(math::max(box.size.x, box.size.y), box.size.z));
	if (largest_dimension > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
		// Cap view distance to make sure you don't accidentally blow up memory when changing parameters
		if (_max_view_distance_voxels > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
			_max_view_distance_voxels = math::min(_max_view_distance_voxels, MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME);
			notify_property_list_changed();
		}
	}
	// TODO Editor gizmo bounds
}

Box3i VoxelTerrain::get_bounds() const {
	return _data->get_bounds();
}

void VoxelTerrain::set_multiplayer_synchronizer(VoxelTerrainMultiplayerSynchronizer *synchronizer) {
	_multiplayer_synchronizer = synchronizer;
}

const VoxelTerrainMultiplayerSynchronizer *VoxelTerrain::get_multiplayer_synchronizer() const {
	return _multiplayer_synchronizer;
}

Vector3i VoxelTerrain::_b_voxel_to_data_block(Vector3 pos) const {
	return _data->voxel_to_block(math::floor_to_int(pos));
}

Vector3i VoxelTerrain::_b_data_block_to_voxel(Vector3i pos) const {
	return _data->block_to_voxel(pos);
}

Ref<VoxelSaveCompletionTracker> VoxelTerrain::_b_save_modified_blocks() {
	std::shared_ptr<AsyncDependencyTracker> tracker = make_shared_instance<AsyncDependencyTracker>();
	save_all_modified_blocks(true, tracker);
	ZN_ASSERT_RETURN_V(tracker != nullptr, Ref<VoxelSaveCompletionTracker>());
	return VoxelSaveCompletionTracker::create(tracker);
}

// Explicitly ask to save a block if it was modified
void VoxelTerrain::_b_save_block(Vector3i p_block_pos) {
	VoxelData::BlockToSave to_save;
	if (_data->consume_block_modifications(p_block_pos, to_save)) {
		_blocks_to_save.push_back(to_save);
	}
}

void VoxelTerrain::_b_set_bounds(AABB aabb) {
	ERR_FAIL_COND(!math::is_valid_size(aabb.size));
	set_bounds(Box3i(math::round_to_int(aabb.position), math::round_to_int(aabb.size)));
}

AABB VoxelTerrain::_b_get_bounds() const {
	const Box3i b = get_bounds();
	return AABB(b.pos, b.size);
}

bool VoxelTerrain::_b_try_set_block_data(Vector3i position, Ref<gd::VoxelBuffer> voxel_data) {
	ERR_FAIL_COND_V(voxel_data.is_null(), false);
	std::shared_ptr<VoxelBufferInternal> buffer = voxel_data->get_buffer_shared();

#ifdef DEBUG_ENABLED
	// It is not allowed to call this function at two different positions with the same voxel buffer
	const StringName &key = VoxelStringNames::get_singleton()._voxel_debug_vt_position;
	if (voxel_data->has_meta(key)) {
		const Vector3i meta_pos = voxel_data->get_meta(key);
		ERR_FAIL_COND_V_MSG(meta_pos != position, false,
				String("Setting the same {0} at different positions is not supported")
						.format(varray(gd::VoxelBuffer::get_class_static())));
	} else {
		voxel_data->set_meta(key, position);
	}
#endif

	return try_set_block_data(position, buffer);
}

PackedInt32Array VoxelTerrain::_b_get_viewer_network_peer_ids_in_area(Vector3i area_origin, Vector3i area_size) const {
	static thread_local std::vector<ViewerID> s_ids;
	std::vector<ViewerID> &viewer_ids = s_ids;
	viewer_ids.clear();
	get_viewers_in_area(viewer_ids, Box3i(area_origin, area_size));

	PackedInt32Array peer_ids;
	peer_ids.resize(viewer_ids.size());
	// Using direct access because when compiling with GodotCpp the array access syntax is different, also it is a bit
	// faster
	int32_t *peer_ids_data = peer_ids.ptrw();
	ZN_ASSERT_RETURN_V(peer_ids_data != nullptr, peer_ids);
	for (size_t i = 0; i < viewer_ids.size(); ++i) {
		const int peer_id = VoxelEngine::get_singleton().get_viewer_network_peer_id(viewer_ids[i]);
		peer_ids_data[i] = peer_id;
	}

	return peer_ids;
}

void VoxelTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &VoxelTerrain::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &VoxelTerrain::get_material_override);

	ClassDB::bind_method(D_METHOD("set_max_view_distance", "distance_in_voxels"), &VoxelTerrain::set_max_view_distance);
	ClassDB::bind_method(D_METHOD("get_max_view_distance"), &VoxelTerrain::get_max_view_distance);

	ClassDB::bind_method(D_METHOD("set_block_enter_notification_enabled", "enabled"),
			&VoxelTerrain::set_block_enter_notification_enabled);
	ClassDB::bind_method(
			D_METHOD("is_block_enter_notification_enabled"), &VoxelTerrain::is_block_enter_notification_enabled);

	ClassDB::bind_method(D_METHOD("set_area_edit_notification_enabled", "enabled"),
			&VoxelTerrain::set_area_edit_notification_enabled);
	ClassDB::bind_method(
			D_METHOD("is_area_edit_notification_enabled"), &VoxelTerrain::is_area_edit_notification_enabled);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_collision_layer"), &VoxelTerrain::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &VoxelTerrain::set_collision_layer);

	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelTerrain::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VoxelTerrain::set_collision_mask);

	ClassDB::bind_method(D_METHOD("get_collision_margin"), &VoxelTerrain::get_collision_margin);
	ClassDB::bind_method(D_METHOD("set_collision_margin", "margin"), &VoxelTerrain::set_collision_margin);

	ClassDB::bind_method(D_METHOD("voxel_to_data_block", "voxel_pos"), &VoxelTerrain::_b_voxel_to_data_block);
	ClassDB::bind_method(D_METHOD("data_block_to_voxel", "block_pos"), &VoxelTerrain::_b_data_block_to_voxel);

	ClassDB::bind_method(D_METHOD("get_data_block_size"), &VoxelTerrain::get_data_block_size);

	ClassDB::bind_method(D_METHOD("get_mesh_block_size"), &VoxelTerrain::get_mesh_block_size);
	ClassDB::bind_method(D_METHOD("set_mesh_block_size", "size"), &VoxelTerrain::set_mesh_block_size);

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelTerrain::_b_get_statistics);
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelTerrain::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelTerrain::_b_save_modified_blocks);
	ClassDB::bind_method(D_METHOD("save_block", "position"), &VoxelTerrain::_b_save_block);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor", "enable"), &VoxelTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelTerrain::is_stream_running_in_editor);

	ClassDB::bind_method(
			D_METHOD("set_automatic_loading_enabled", "enable"), &VoxelTerrain::set_automatic_loading_enabled);
	ClassDB::bind_method(D_METHOD("is_automatic_loading_enabled"), &VoxelTerrain::is_automatic_loading_enabled);

	// TODO Rename `_voxel_bounds`
	ClassDB::bind_method(D_METHOD("set_bounds"), &VoxelTerrain::_b_set_bounds);
	ClassDB::bind_method(D_METHOD("get_bounds"), &VoxelTerrain::_b_get_bounds);

	ClassDB::bind_method(D_METHOD("try_set_block_data", "position", "voxels"), &VoxelTerrain::_b_try_set_block_data);

	ClassDB::bind_method(D_METHOD("get_viewer_network_peer_ids_in_area", "area_origin", "area_size"),
			&VoxelTerrain::_b_get_viewer_network_peer_ids_in_area);

	ClassDB::bind_method(D_METHOD("has_data_block", "block_position"), &VoxelTerrain::has_data_block);

#ifdef ZN_GODOT
	GDVIRTUAL_BIND(_on_data_block_entered, "info");
	GDVIRTUAL_BIND(_on_area_edited, "area_origin", "area_size");
#endif

	ADD_GROUP("Bounds", "");

	ADD_PROPERTY(PropertyInfo(Variant::AABB, "bounds"), "set_bounds", "get_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_view_distance"), "set_max_view_distance", "get_max_view_distance");

	ADD_GROUP("Collisions", "");

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "generate_collisions"), "set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer",
			"get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask",
			"get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_margin"), "set_collision_margin", "get_collision_margin");

	ADD_GROUP("Materials", "");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE,
						 String("{0},{1}").format(
								 varray(BaseMaterial3D::get_class_static(), ShaderMaterial::get_class_static()))),
			"set_material_override", "get_material_override");

	ADD_GROUP("Networking", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "block_enter_notification_enabled"),
			"set_block_enter_notification_enabled", "is_block_enter_notification_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "area_edit_notification_enabled"), "set_area_edit_notification_enabled",
			"is_area_edit_notification_enabled");

	// This may be set to false in multiplayer designs where the server is the one sending the blocks
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "automatic_loading_enabled"), "set_automatic_loading_enabled",
			"is_automatic_loading_enabled");

	ADD_GROUP("Advanced", "");

	// TODO Should probably be in the parent class?
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"), "set_run_stream_in_editor",
			"is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_block_size"), "set_mesh_block_size", "get_mesh_block_size");

	// TODO Add back access to block, but with an API securing multithreaded access
	ADD_SIGNAL(MethodInfo("block_loaded", PropertyInfo(Variant::VECTOR3I, "position")));
	ADD_SIGNAL(MethodInfo("block_unloaded", PropertyInfo(Variant::VECTOR3I, "position")));

	ADD_SIGNAL(MethodInfo("mesh_block_entered", PropertyInfo(Variant::VECTOR3I, "position")));
	ADD_SIGNAL(MethodInfo("mesh_block_exited", PropertyInfo(Variant::VECTOR3I, "position")));
}

} // namespace zylann::voxel
