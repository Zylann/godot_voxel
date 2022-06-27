#include "voxel_terrain.h"
#include "../../constants/voxel_constants.h"
#include "../../constants/voxel_string_names.h"
#include "../../edition/voxel_tool_terrain.h"
#include "../../meshers/blocky/voxel_mesher_blocky.h"
#include "../../server/generate_block_task.h"
#include "../../server/load_block_data_task.h"
#include "../../server/mesh_block_task.h"
#include "../../server/save_block_data_task.h"
#include "../../server/voxel_server.h"
#include "../../server/voxel_server_updater.h"
#include "../../storage/voxel_buffer_gd.h"
#include "../../util/container_funcs.h"
#include "../../util/macros.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "../instancing/voxel_instancer.h"
#include "../voxel_data_block_enter_info.h"

#include <core/config/engine.h>
#include <core/core_string_names.h>
#include <scene/3d/mesh_instance_3d.h>
#include <scene/resources/concave_polygon_shape_3d.h>

namespace zylann::voxel {

VoxelTerrain::VoxelTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	set_notify_transform(true);

	// TODO Should it actually be finite for better discovery?
	// Infinite by default
	_bounds_in_voxels = Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT));

	_streaming_dependency = make_shared_instance<StreamingDependency>();
	_meshing_dependency = make_shared_instance<MeshingDependency>();

	struct ApplyMeshUpdateTask : public ITimeSpreadTask {
		void run(TimeSpreadTaskContext &ctx) override {
			if (!VoxelServer::get_singleton().is_volume_valid(volume_id)) {
				// The node can have been destroyed while this task was still pending
				ZN_PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
				return;
			}
			self->apply_mesh_update(data);
		}
		uint32_t volume_id = 0;
		VoxelTerrain *self = nullptr;
		VoxelServer::BlockMeshOutput data;
	};

	// Mesh updates are spread over frames by scheduling them in a task runner of VoxelServer,
	// but instead of using a reception buffer we use a callback,
	// because this kind of task scheduling would otherwise delay the update by 1 frame
	VoxelServer::VolumeCallbacks callbacks;
	callbacks.data = this;
	callbacks.mesh_output_callback = [](void *cb_data, VoxelServer::BlockMeshOutput &ob) {
		VoxelTerrain *self = reinterpret_cast<VoxelTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->_volume_id;
		task->self = self;
		task->data = std::move(ob);
		VoxelServer::get_singleton().push_main_thread_time_spread_task(task);
	};
	callbacks.data_output_callback = [](void *cb_data, VoxelServer::BlockDataOutput &ob) {
		VoxelTerrain *self = reinterpret_cast<VoxelTerrain *>(cb_data);
		self->apply_data_block_response(ob);
	};

	_volume_id = VoxelServer::get_singleton().add_volume(callbacks);

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
	VoxelServer::get_singleton().remove_volume(_volume_id);
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
	if (p_stream == _stream) {
		return;
	}

	_stream = p_stream;

	StreamingDependency::reset(_streaming_dependency, _stream, _generator);

#ifdef TOOLS_ENABLED
	if (_stream.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _stream->get_script();
			if (script.is_valid()) {
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
	return _stream;
}

void VoxelTerrain::set_generator(Ref<VoxelGenerator> p_generator) {
	if (p_generator == _generator) {
		return;
	}

	_generator = p_generator;

	MeshingDependency::reset(_meshing_dependency, _mesher, p_generator);
	StreamingDependency::reset(_streaming_dependency, _stream, p_generator);

#ifdef TOOLS_ENABLED
	if (_generator.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _generator->get_script();
			if (script.is_valid()) {
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
	return _generator;
}

/*void VoxelTerrain::set_data_block_size_po2(unsigned int p_block_size_po2) {
	ERR_FAIL_COND(p_block_size_po2 < 1);
	ERR_FAIL_COND(p_block_size_po2 > 32);

	unsigned int block_size_po2 = p_block_size_po2;
	if (_stream.is_valid()) {
		block_size_po2 = _stream->get_block_size_po2();
	}

	if (block_size_po2 == get_data_block_size_pow2()) {
		return;
	}

	_set_block_size_po2(block_size_po2);
	_on_stream_params_changed();
}*/

void VoxelTerrain::_set_block_size_po2(int p_block_size_po2) {
	_data_map.create(p_block_size_po2, 0);
}

unsigned int VoxelTerrain::get_data_block_size_pow2() const {
	return _data_map.get_block_size_pow2();
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
		_mesh_map.for_each_block([&instancer](VoxelMeshBlockVT &block) { //
			instancer.on_mesh_block_exit(block.position, 0);
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

	// VoxelServer::get_singleton().set_volume_render_block_size(_volume_id, mesh_block_size);

	// No update on bounds because we can support a mismatch, as long as it is a multiple of data block size
	//set_bounds(_bounds_in_voxels);
}

void VoxelTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	if (_stream.is_valid()) {
		const int stream_block_size_po2 = _stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);
	}

	// VoxelServer::get_singleton().set_volume_data_block_size(_volume_id, 1 << get_data_block_size_pow2());
	// VoxelServer::get_singleton().set_volume_render_block_size(_volume_id, 1 << get_mesh_block_size_pow2());

	// The whole map might change, so regenerate it
	reset_map();

	if ((_stream.is_valid() || _generator.is_valid()) &&
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

Ref<VoxelMesher> VoxelTerrain::get_mesher() const {
	return _mesher;
}

void VoxelTerrain::set_mesher(Ref<VoxelMesher> mesher) {
	if (mesher == _mesher) {
		return;
	}

	_mesher = mesher;

	MeshingDependency::reset(_meshing_dependency, _mesher, _generator);

	stop_updater();

	if (_mesher.is_valid()) {
		start_updater();
		// Voxel appearance might completely change
		remesh_all_blocks();
	}

	update_configuration_warnings();
}

void VoxelTerrain::get_viewers_in_area(std::vector<int> &out_viewer_ids, Box3i voxel_box) const {
	const Box3i block_box = voxel_box.downscaled(_data_map.get_block_size());

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
	const Box3i bounds_in_data_blocks = _bounds_in_voxels.downscaled(get_data_block_size());
	// Pad by 1 because meshing needs neighbors
	const Box3i data_box =
			Box3i(mesh_block.position * render_to_data_factor, Vector3iUtil::create(render_to_data_factor))
					.padded(1)
					.clipped(bounds_in_data_blocks);

	// If we get an empty box at this point, something is wrong with the caller
	ERR_FAIL_COND(data_box.is_empty());

	// Check if we have the data
	const bool data_available = data_box.all_cells_match([this](Vector3i bpos) { //
		return _data_map.has_block(bpos);
	});

	if (data_available) {
		// Regardless of if the updater is updating the block already,
		// the block could have been modified again so we schedule another update
		mesh_block.set_mesh_state(VoxelMeshBlockVT::MESH_UPDATE_NOT_SENT);
		_blocks_pending_update.push_back(mesh_block.position);
	}
}

void VoxelTerrain::view_data_block(Vector3i bpos, uint32_t viewer_id, bool require_notification) {
	VoxelDataBlock *block = _data_map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
		auto loading_block_it = _loading_blocks.find(bpos);

		if (loading_block_it == _loading_blocks.end()) {
			// First viewer to request it
			LoadingBlock new_loading_block;
			new_loading_block.viewers.add();

			if (require_notification) {
				new_loading_block.viewers_to_notify.push_back(viewer_id);
			}

			// Schedule a loading request
			_loading_blocks.insert({ bpos, new_loading_block });
			_blocks_pending_load.push_back(bpos);

		} else {
			// More viewers
			LoadingBlock &loading_block = loading_block_it->second;
			loading_block.viewers.add();

			if (require_notification) {
				loading_block.viewers_to_notify.push_back(viewer_id);
			}
		}

	} else {
		// The block is loaded
		block->viewers.add();

		if (require_notification) {
			notify_data_block_enter(*block, bpos, viewer_id);
		}

		// TODO viewers with varying flags during the game is not supported at the moment.
		// They have to be re-created, which may cause world re-load...
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

void VoxelTerrain::unview_data_block(Vector3i bpos) {
	VoxelDataBlock *block = _data_map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
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

	} else {
		// The block is loaded
		block->viewers.remove();
		if (block->viewers.get() == 0) {
			// The block itself is no longer wanted
			unload_data_block(bpos);
		}
	}
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

namespace {
struct ScheduleSaveAction {
	std::vector<VoxelTerrain::BlockToSave> &blocks_to_save;
	bool with_copy;

	void operator()(const Vector3i &bpos, VoxelDataBlock &block) {
		// TODO Don't ask for save if the stream doesn't support it!
		if (block.is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelTerrain::BlockToSave b;
			// If a modified block has no voxels, it is equivalent to removing the block from the stream
			if (block.has_voxels()) {
				if (with_copy) {
					RWLockRead lock(block.get_voxels().get_lock());
					b.voxels = make_shared_instance<VoxelBufferInternal>();
					block.get_voxels_const().duplicate_to(*b.voxels, true);
				} else {
					b.voxels = block.get_voxels_shared();
				}
			}
			b.position = bpos;
			blocks_to_save.push_back(b);
			block.set_modified(false);
		}
	}
};
} // namespace

void VoxelTerrain::unload_data_block(Vector3i bpos) {
	const bool save = _stream.is_valid() && (!Engine::get_singleton()->is_editor_hint() || _run_stream_in_editor);

	_data_map.remove_block(bpos, [this, save, bpos](VoxelDataBlock &block) {
		emit_data_block_unloaded(block, bpos);
		if (save) {
			// Note: no need to copy the block because it gets removed from the map anyways
			ScheduleSaveAction{ _blocks_to_save, false }(bpos, block);
		}
	});

	_loading_blocks.erase(bpos);

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block
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
}

void VoxelTerrain::save_all_modified_blocks(bool with_copy) {
	// That may cause a stutter, so should be used when the player won't notice
	_data_map.for_each_block(ScheduleSaveAction{ _blocks_to_save, with_copy });

	if (_stream.is_valid() && _instancer != nullptr && _stream->supports_instance_blocks()) {
		_instancer->save_all_modified_blocks();
	}

	// And flush immediately
	send_block_data_requests();
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

// This function is primarily intented for editor use cases at the moment.
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

	// VoxelServer::get_singleton().set_volume_mesher(_volume_id, _mesher);
}

void VoxelTerrain::stop_updater() {
	// Invalidate pending tasks
	MeshingDependency::reset(_meshing_dependency, _mesher, _generator);

	// VoxelServer::get_singleton().set_volume_mesher(_volume_id, Ref<VoxelMesher>());

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
	if (_data_map.has_block(block_position)) {
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
	const Box3i block_box(_data_map.block_to_voxel(block_position), Vector3iUtil::create(_data_map.get_block_size()));
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
	// VoxelServer::get_singleton().set_volume_stream(_volume_id, _stream);
	// VoxelServer::get_singleton().set_volume_generator(_volume_id, _generator);
}

void VoxelTerrain::stop_streamer() {
	// Invalidate pending tasks
	StreamingDependency::reset(_streaming_dependency, _stream, _generator);
	// VoxelServer::get_singleton().set_volume_stream(_volume_id, Ref<VoxelStream>());
	// VoxelServer::get_singleton().set_volume_generator(_volume_id, Ref<VoxelGenerator>());
	_loading_blocks.clear();
	_blocks_pending_load.clear();
}

void VoxelTerrain::reset_map() {
	// Discard everything, to reload it all

	_data_map.for_each_block([this](const Vector3i &bpos, VoxelDataBlock &block) { //
		emit_data_block_unloaded(block, bpos);
	});
	_data_map.create(get_data_block_size_pow2(), 0);

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
	box_in_voxels.clip(_bounds_in_voxels);

	// Mark data as modified
	const Box3i data_box = box_in_voxels.downscaled(get_data_block_size());
	data_box.for_each_cell([this](Vector3i pos) {
		VoxelDataBlock *block = _data_map.get_block(pos);
		// The edit can happen next to a boundary
		if (block != nullptr) {
			block->set_modified(true);
			block->set_edited(true);
		}
	});

	if (_area_edit_notification_enabled) {
		GDVIRTUAL_CALL(_on_area_edited, box_in_voxels.pos, box_in_voxels.size);
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
			break;

		case NOTIFICATION_PROCESS:
			// Can't do that in enter tree because Godot is "still setting up children".
			// Can't do that in ready either because Godot says node state is locked.
			// This hack is quite miserable.
			VoxelServerUpdater::ensure_existence(get_tree());

			_process();
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
			// VoxelServer::get_singleton().set_volume_transform(_volume_id, transform);

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

static void request_block_load(uint32_t volume_id, std::shared_ptr<StreamingDependency> stream_dependency,
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

		VoxelServer::get_singleton().push_async_io_task(task);

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

		VoxelServer::get_singleton().push_async_task(task);
	}
}

static void request_voxel_block_save(uint32_t volume_id, const std::shared_ptr<VoxelBufferInternal> &voxels,
		Vector3i block_pos, std::shared_ptr<StreamingDependency> &stream_dependency, unsigned int data_block_size) {
	//
	ZN_ASSERT(stream_dependency != nullptr);
	ZN_ASSERT_RETURN(stream_dependency->stream.is_valid());

	SaveBlockDataTask *task =
			ZN_NEW(SaveBlockDataTask(volume_id, block_pos, 0, data_block_size, voxels, stream_dependency));

	// No priority data, saving doesnt need sorting

	VoxelServer::get_singleton().push_async_io_task(task);
}

void VoxelTerrain::send_block_data_requests() {
	ZN_PROFILE_SCOPE();

	std::shared_ptr<PriorityDependency::ViewersData> shared_viewers_data =
			VoxelServer::get_singleton().get_shared_viewers_data_from_default_world();

	const Transform3D volume_transform = get_global_transform();

	// Blocks to load
	for (size_t i = 0; i < _blocks_pending_load.size(); ++i) {
		const Vector3i block_pos = _blocks_pending_load[i];
		// TODO Batch request
		request_block_load(_volume_id, _streaming_dependency, get_data_block_size(), block_pos, shared_viewers_data,
				volume_transform, _instancer != nullptr);
	}

	// Blocks to save
	if (_stream.is_valid()) {
		for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
			ZN_PRINT_VERBOSE(format("Requesting save of block {}", _blocks_to_save[i].position));
			const BlockToSave b = _blocks_to_save[i];
			// TODO Batch request
			request_voxel_block_save(_volume_id, b.voxels, b.position, _streaming_dependency, get_data_block_size());
		}
	} else {
		if (_blocks_to_save.size() > 0) {
			ZN_PRINT_VERBOSE(format("Not saving {} blocks because no stream is assigned", _blocks_to_save.size()));
		}
	}

	//print_line(String("Sending {0} block requests").format(varray(input.blocks_to_emerge.size())));
	_blocks_pending_load.clear();
	_blocks_to_save.clear();
}

void VoxelTerrain::emit_data_block_loaded(const VoxelDataBlock &block, Vector3i bpos) {
	// Not sure about exposing buffers directly... some stuff on them is useful to obtain directly,
	// but also it allows scripters to mess with voxels in a way they should not.
	// Example: modifying voxels without locking them first, while another thread may be reading them at the same
	// time. The same thing could happen the other way around (threaded task modifying voxels while you try to read
	// them). It isn't planned to expose VoxelBuffer locks because there are too many of them, it may likely shift
	// to another system in the future, and might even be changed to no longer inherit Reference. So unless this is
	// absolutely necessary, buffers aren't exposed. Workaround: use VoxelTool
	//const Variant vbuffer = block->voxels;
	//const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton().block_loaded, bpos);
}

void VoxelTerrain::emit_data_block_unloaded(const VoxelDataBlock &block, Vector3i bpos) {
	// const Variant vbuffer = block->voxels;
	// const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton().block_unloaded, bpos);
}

bool VoxelTerrain::try_get_paired_viewer_index(uint32_t id, size_t &out_i) const {
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
void VoxelTerrain::notify_data_block_enter(VoxelDataBlock &block, Vector3i bpos, uint32_t viewer_id) {
	if (!VoxelServer::get_singleton().viewer_exists(viewer_id)) {
		// The viewer might have been removed between the moment we requested the block and the moment we finished
		// loading it
		return;
	}
	if (_data_block_enter_info_obj == nullptr) {
		_data_block_enter_info_obj = gd_make_unique<VoxelDataBlockEnterInfo>();
	}
	_data_block_enter_info_obj->network_peer_id = VoxelServer::get_singleton().get_viewer_network_peer_id(viewer_id);
	_data_block_enter_info_obj->voxel_block = &block;
	_data_block_enter_info_obj->block_position = bpos;

	if (!GDVIRTUAL_CALL(_on_data_block_entered, _data_block_enter_info_obj.get())) {
		WARN_PRINT_ONCE("VoxelTerrain::_on_data_block_entered is unimplemented!");
	}
}

void VoxelTerrain::_process() {
	ZN_PROFILE_SCOPE();
	process_viewers();
	//process_received_data_blocks();
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
			if (!VoxelServer::get_singleton().viewer_exists(p.id)) {
				ZN_PRINT_VERBOSE("Detected destroyed viewer in VoxelTerrain");
				// Interpret removal as nullified view distance so the same code handling loading of blocks
				// will be used to unload those viewed by this viewer.
				// We'll actually remove unpaired viewers in a second pass.
				p.state.view_distance_voxels = 0;
				unpaired_viewer_indexes.push_back(i);
			}
		}

		const Transform3D local_to_world_transform = get_global_transform();
		const Transform3D world_to_local_transform = local_to_world_transform.affine_inverse();

		// Note, this does not support non-uniform scaling
		// TODO There is probably a better way to do this
		const float view_distance_scale = world_to_local_transform.basis.xform(Vector3(1, 0, 0)).length();

		const Box3i bounds_in_data_blocks = _bounds_in_voxels.downscaled(get_data_block_size());
		const Box3i bounds_in_mesh_blocks = _bounds_in_voxels.downscaled(get_mesh_block_size());

		struct UpdatePairedViewer {
			VoxelTerrain &self;
			const Box3i bounds_in_data_blocks;
			const Box3i bounds_in_mesh_blocks;
			const Transform3D world_to_local_transform;
			const float view_distance_scale;

			inline void operator()(const VoxelServer::Viewer &viewer, uint32_t viewer_id) {
				size_t paired_viewer_index;
				if (!self.try_get_paired_viewer_index(viewer_id, paired_viewer_index)) {
					PairedViewer p;
					p.id = viewer_id;
					paired_viewer_index = self._paired_viewers.size();
					self._paired_viewers.push_back(p);
				}

				PairedViewer &paired_viewer = self._paired_viewers[paired_viewer_index];
				paired_viewer.prev_state = paired_viewer.state;
				PairedViewer::State &state = paired_viewer.state;

				const unsigned int view_distance_voxels =
						static_cast<unsigned int>(static_cast<float>(viewer.view_distance) * view_distance_scale);
				const Vector3 local_position = world_to_local_transform.xform(viewer.world_position);

				state.view_distance_voxels = math::min(view_distance_voxels, self._max_view_distance_voxels);
				state.local_position_voxels = math::floor_to_int(local_position);
				state.requires_collisions = VoxelServer::get_singleton().is_viewer_requiring_collisions(viewer_id);
				state.requires_meshes = VoxelServer::get_singleton().is_viewer_requiring_visuals(viewer_id);

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

		// New viewers and updates
		UpdatePairedViewer u{ *this, bounds_in_data_blocks, bounds_in_mesh_blocks, world_to_local_transform,
			view_distance_scale };
		VoxelServer::get_singleton().for_each_viewer(u);
	}

	const bool can_load_blocks = (_automatic_loading_enabled && (_stream.is_valid() || _generator.is_valid())) &&
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
					ZN_PROFILE_SCOPE();

					const bool require_notifications = _block_enter_notification_enabled &&
							VoxelServer::get_singleton().is_viewer_requiring_data_block_notifications(viewer.id);

					// Unview blocks that just fell out of range
					prev_data_box.difference(new_data_box, [this](Box3i out_of_range_box) {
						out_of_range_box.for_each_cell([this](Vector3i bpos) { //
							unview_data_block(bpos);
						});
					});

					// View blocks that just entered the range
					if (can_load_blocks) {
						new_data_box.difference(
								prev_data_box, [this, &viewer, require_notifications](Box3i box_to_load) {
									box_to_load.for_each_cell([this, &viewer, require_notifications](Vector3i bpos) {
										// Load or update block
										view_data_block(bpos, viewer.id, require_notifications);
									});
								});
					}
				}
			}

			{
				const Box3i &new_mesh_box = viewer.state.mesh_box;
				const Box3i &prev_mesh_box = viewer.prev_state.mesh_box;

				if (prev_mesh_box != new_mesh_box) {
					ZN_PROFILE_SCOPE();

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
		ZN_PRINT_VERBOSE("Unpairing viewer from VoxelTerrain");
		// Iterating backward so indexes of paired viewers will not change because of the removal
		const size_t vi = unpaired_viewer_indexes[unpaired_viewer_indexes.size() - i - 1];
		_paired_viewers[vi] = _paired_viewers.back();
		_paired_viewers.pop_back();
	}

	// It's possible the user didn't set a stream yet, or it is turned off
	if (can_load_blocks) {
		send_block_data_requests();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();
}

void VoxelTerrain::apply_data_block_response(VoxelServer::BlockDataOutput &ob) {
	ZN_PROFILE_SCOPE();

	//print_line(String("Receiving {0} blocks").format(varray(output.emerged_blocks.size())));

	if (ob.type == VoxelServer::BlockDataOutput::TYPE_SAVED) {
		if (ob.dropped) {
			ERR_PRINT(String("Could not save block {0}").format(varray(ob.position)));
		}
		return;
	}

	CRASH_COND(ob.type != VoxelServer::BlockDataOutput::TYPE_LOADED &&
			ob.type != VoxelServer::BlockDataOutput::TYPE_GENERATED);

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

	const Vector3i expected_block_size = Vector3iUtil::create(_data_map.get_block_size());
	if (ob.voxels->get_size() != expected_block_size) {
		// Voxel block size is incorrect, drop it
		ERR_PRINT(String("Block size obtained from stream is different from expected size. "
						 "Expected {0}, got {1}")
						  .format(varray(expected_block_size, ob.voxels->get_size())));
		++_stats.dropped_block_loads;
		return;
	}

	// Create or update block data
	VoxelDataBlock *block = _data_map.get_block(block_pos);
	const bool was_not_loaded = block == nullptr;
	block = _data_map.set_block_buffer(block_pos, ob.voxels, true);
	CRASH_COND(block == nullptr);

	block->set_edited(ob.type == VoxelServer::BlockDataOutput::TYPE_LOADED);

	if (was_not_loaded) {
		// Set viewers count that are currently expecting the block
		block->viewers = loading_block.viewers;
	}

	emit_data_block_loaded(*block, block_pos);

	for (unsigned int i = 0; i < loading_block.viewers_to_notify.size(); ++i) {
		const uint32_t viewer_id = loading_block.viewers_to_notify[i];
		notify_data_block_enter(*block, block_pos, viewer_id);
	}

	// The block itself might not be suitable for meshing yet, but blocks surrounding it might be now
	{
		ZN_PROFILE_SCOPE();
		try_schedule_mesh_update_from_data(
				Box3i(_data_map.block_to_voxel(block_pos), Vector3iUtil::create(get_data_block_size())));
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

	const Vector3i expected_block_size = Vector3iUtil::create(_data_map.get_block_size());
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
		return false;
	}

	// Cancel loading version if any
	_loading_blocks.erase(position);

	// Create or update block data
	VoxelDataBlock *block = _data_map.set_block_buffer(position, voxel_data, true);
	CRASH_COND(block == nullptr);
	block->viewers = refcount;
	// TODO How to set the `edited` flag? Does it matter in use cases for this function?

	// The block itself might not be suitable for meshing yet, but blocks surrounding it might be now
	try_schedule_mesh_update_from_data(
			Box3i(_data_map.block_to_voxel(position), Vector3iUtil::create(get_data_block_size())));

	return true;
}

bool VoxelTerrain::has_data_block(Vector3i position) const {
	return _data_map.has_block(position);
}

void VoxelTerrain::process_meshing() {
	ProfilingClock profiling_clock;

	_stats.dropped_block_meshs = 0;
	const Transform3D volume_transform = get_global_transform();
	std::shared_ptr<PriorityDependency::ViewersData> shared_viewers_data =
			VoxelServer::get_singleton().get_shared_viewers_data_from_default_world();

	// Send mesh updates
	{
		ZN_PROFILE_SCOPE();

		//const int used_channels_mask = get_used_channels_mask();
		const int mesh_to_data_factor = get_mesh_block_size() / get_data_block_size();

		for (size_t bi = 0; bi < _blocks_pending_update.size(); ++bi) {
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
				const VoxelDataBlock *data_block = _data_map.get_block(anchor_pos);
				ERR_CONTINUE(data_block == nullptr);
			}
#endif

			//print_line(String("DDD request {0}").format(varray(mesh_request.render_block_position.to_vec3())));
			// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
			MeshBlockTask *task = ZN_NEW(MeshBlockTask);
			task->volume_id = _volume_id;
			task->position = mesh_block_pos;
			task->lod_index = 0;
			task->meshing_dependency = _meshing_dependency;
			task->data_block_size = get_data_block_size();
			task->collision_hint = _generate_collisions;

			// This iteration order is specifically chosen to match VoxelServer and threaded access
			task->blocks_count = 0;
			data_box.for_each_cell_zxy([this, task](Vector3i data_block_pos) {
				VoxelDataBlock *data_block = _data_map.get_block(data_block_pos);
				if (data_block != nullptr && data_block->has_voxels()) {
					task->blocks[task->blocks_count] = data_block->get_voxels_shared();
				}
				++task->blocks_count;
			});

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

			init_sparse_grid_priority_dependency(task->priority_dependency, task->position, get_mesh_block_size(),
					shared_viewers_data, volume_transform);

			VoxelServer::get_singleton().push_async_task(task);

			mesh_block->set_mesh_state(VoxelMeshBlockVT::MESH_UPDATE_SENT);
		}

		_blocks_pending_update.clear();
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	//print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") +
	//String::num(_block_update_queue.size()));
}

void VoxelTerrain::apply_mesh_update(const VoxelServer::BlockMeshOutput &ob) {
	ZN_PROFILE_SCOPE();
	//print_line(String("DDD receive {0}").format(varray(ob.position.to_vec3())));

	VoxelMeshBlockVT *block = _mesh_map.get_block(ob.position);
	if (block == nullptr) {
		//print_line("- no longer loaded");
		// That block is no longer loaded, drop the result
		++_stats.dropped_block_meshs;
		return;
	}

	if (ob.type == VoxelServer::BlockMeshOutput::TYPE_DROPPED) {
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

	const bool gen_collisions = _generate_collisions && block->collision_viewers.get() > 0;
	const bool use_render_mesh_as_collider = gen_collisions && !_mesher->is_generating_collision_surface();
	std::vector<Array> render_surfaces;

	int gd_surface_index = 0;
	for (unsigned int surface_index = 0; surface_index < ob.surfaces.surfaces.size(); ++surface_index) {
		const VoxelMesher::Output::Surface &surface = ob.surfaces.surfaces[surface_index];
		Array arrays = surface.arrays;
		if (arrays.is_empty()) {
			continue;
		}

		CRASH_COND(arrays.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(arrays)) {
			continue;
		}

		if (use_render_mesh_as_collider) {
			render_surfaces.push_back(arrays);
		}

		if (mesh.is_null()) {
			mesh.instantiate();
		}

		mesh->add_surface_from_arrays(
				ob.surfaces.primitive_type, arrays, Array(), Dictionary(), ob.surfaces.mesh_flags);

		Ref<Material> material = _mesher->get_material_by_index(surface_index);
		mesh->surface_set_material(gd_surface_index, material);
		++gd_surface_index;
	}

	if (mesh.is_valid() && is_mesh_empty(**mesh)) {
		mesh = Ref<Mesh>();
		render_surfaces.clear();
	}

	if (_instancer != nullptr) {
		if (mesh.is_null() && block != nullptr) {
			// No surface anymore in this block
			_instancer->on_mesh_block_exit(ob.position, ob.lod);
		}
		if (ob.surfaces.surfaces.size() > 0 && mesh.is_valid() && !block->has_mesh()) {
			// TODO The mesh could come from an edited region!
			// We would have to know if specific voxels got edited, or different from the generator
			// TODO Support multi-surfaces in VoxelInstancer
			_instancer->on_mesh_block_enter(ob.position, ob.lod, ob.surfaces.surfaces[0].arrays);
		}
	}

	block->set_mesh(mesh, DirectMeshInstance::GIMode(get_gi_mode()));

	if (_material_override.is_valid()) {
		block->set_material_override(_material_override);
	}

	if (gen_collisions) {
		Ref<Shape3D> collision_shape = make_collision_shape_from_mesher_output(ob.surfaces, **_mesher);
		block->set_collision_shape(
				collision_shape, get_tree()->is_debugging_collisions_hint(), this, _collision_margin);

		block->set_collision_layer(_collision_layer);
		block->set_collision_mask(_collision_mask);
	}

	block->set_visible(true);
	block->set_parent_visible(is_visible());
	block->set_parent_transform(get_global_transform());
	// TODO We dont set MESH_UP_TO_DATE anywhere, but it seems to work?
}

Ref<VoxelTool> VoxelTerrain::get_voxel_tool() {
	Ref<VoxelTool> vt = memnew(VoxelToolTerrain(this));
	const int used_channels_mask = get_used_channels_mask();
	// Auto-pick first used channel
	for (int channel = 0; channel < VoxelBufferInternal::MAX_CHANNELS; ++channel) {
		if ((used_channels_mask & (1 << channel)) != 0) {
			vt->set_channel(channel);
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
	_bounds_in_voxels =
			box.clipped(Box3i::from_center_extents(Vector3i(), Vector3iUtil::create(constants::MAX_VOLUME_EXTENT)));

	// Round to block size
	_bounds_in_voxels = _bounds_in_voxels.snapped(get_data_block_size());

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
	return _bounds_in_voxels;
}

Vector3i VoxelTerrain::_b_voxel_to_data_block(Vector3 pos) const {
	return _data_map.voxel_to_block(math::floor_to_int(pos));
}

Vector3i VoxelTerrain::_b_data_block_to_voxel(Vector3i pos) const {
	return _data_map.block_to_voxel(pos);
}

void VoxelTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

// Explicitely ask to save a block if it was modified
void VoxelTerrain::_b_save_block(Vector3i p_block_pos) {
	VoxelDataBlock *block = _data_map.get_block(p_block_pos);
	ERR_FAIL_COND(block == nullptr);

	if (!block->is_modified()) {
		return;
	}

	ScheduleSaveAction{ _blocks_to_save, true }(p_block_pos, *block);
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
	static thread_local std::vector<int> s_ids;
	std::vector<int> &viewer_ids = s_ids;
	viewer_ids.clear();
	get_viewers_in_area(viewer_ids, Box3i(area_origin, area_size));

	PackedInt32Array peer_ids;
	peer_ids.resize(viewer_ids.size());
	for (size_t i = 0; i < viewer_ids.size(); ++i) {
		const int peer_id = VoxelServer::get_singleton().get_viewer_network_peer_id(viewer_ids[i]);
		peer_ids.write[i] = peer_id;
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

	GDVIRTUAL_BIND(_on_data_block_entered, "info");
	GDVIRTUAL_BIND(_on_area_edited, "area_origin", "area_size");

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

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override"), "set_material_override", "get_material_override");

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
	ADD_SIGNAL(MethodInfo(VoxelStringNames::get_singleton().block_loaded, PropertyInfo(Variant::VECTOR3, "position")));
	ADD_SIGNAL(
			MethodInfo(VoxelStringNames::get_singleton().block_unloaded, PropertyInfo(Variant::VECTOR3, "position")));
}

} // namespace zylann::voxel
