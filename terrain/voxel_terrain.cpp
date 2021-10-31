#include "voxel_terrain.h"
#include "../constants/voxel_constants.h"
#include "../constants/voxel_string_names.h"
#include "../edition/voxel_tool_terrain.h"
#include "../server/voxel_server.h"
#include "../util/funcs.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/profiling_clock.h"

#include <core/core_string_names.h>
#include <core/engine.h>
#include <scene/3d/mesh_instance.h>

VoxelTerrain::VoxelTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	set_notify_transform(true);

	// TODO Should it actually be finite for better discovery?
	// Infinite by default
	_bounds_in_voxels = Box3i::from_center_extents(Vector3i(0), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT));

	struct ApplyMeshUpdateTask : public IVoxelTimeSpreadTask {
		void run() override {
			if (!VoxelServer::get_singleton()->is_volume_valid(volume_id)) {
				// The node can have been destroyed while this task was still pending
				PRINT_VERBOSE("Cancelling ApplyMeshUpdateTask, volume_id is invalid");
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
	_reception_buffers.callback_data = this;
	_reception_buffers.mesh_output_callback = [](void *cb_data, const VoxelServer::BlockMeshOutput &ob) {
		VoxelTerrain *self = reinterpret_cast<VoxelTerrain *>(cb_data);
		ApplyMeshUpdateTask *task = memnew(ApplyMeshUpdateTask);
		task->volume_id = self->_volume_id;
		task->self = self;
		task->data = ob;
		VoxelServer::get_singleton()->push_time_spread_task(task);
	};

	_volume_id = VoxelServer::get_singleton()->add_volume(&_reception_buffers, VoxelServer::VOLUME_SPARSE_GRID);

	// For ease of use in editor
	Ref<VoxelMesherBlocky> default_mesher;
	default_mesher.instance();
	_mesher = default_mesher;
}

VoxelTerrain::~VoxelTerrain() {
	PRINT_VERBOSE("Destroying VoxelTerrain");
	VoxelServer::get_singleton()->remove_volume(_volume_id);
}

// TODO See if there is a way to specify materials in voxels directly?

bool VoxelTerrain::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name.operator String().begins_with("material/")) {
		unsigned int idx = p_name.operator String().get_slicec('/', 1).to_int();
		ERR_FAIL_COND_V(idx >= VoxelMesherBlocky::MAX_MATERIALS || idx < 0, false);
		set_material(idx, p_value);
		return true;
	}

	return false;
}

bool VoxelTerrain::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name.operator String().begins_with("material/")) {
		unsigned int idx = p_name.operator String().get_slicec('/', 1).to_int();
		ERR_FAIL_COND_V(idx >= VoxelMesherBlocky::MAX_MATERIALS || idx < 0, false);
		r_ret = get_material(idx);
		return true;
	}

	return false;
}

void VoxelTerrain::_get_property_list(List<PropertyInfo> *p_list) const {
	// Need to add a group here because otherwise it appears under the last group declared in `_bind_methods`
	p_list->push_back(PropertyInfo(Variant::NIL, "Materials", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP));
	for (unsigned int i = 0; i < VoxelMesherBlocky::MAX_MATERIALS; ++i) {
		p_list->push_back(PropertyInfo(
				Variant::OBJECT, "material/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial"));
	}
}

void VoxelTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream == _stream) {
		return;
	}

	_stream = p_stream;

#ifdef TOOLS_ENABLED
	if (_stream.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _stream->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_run_stream_in_editor = false;
				_change_notify();
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

#ifdef TOOLS_ENABLED
	if (_generator.is_valid()) {
		if (Engine::get_singleton()->is_editor_hint()) {
			Ref<Script> script = _generator->get_script();
			if (script.is_valid()) {
				// Safety check. It's too easy to break threads by making a script reload.
				// You can turn it back on, but be careful.
				_run_stream_in_editor = false;
				_change_notify();
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
	return _mesh_map.get_block_size_pow2();
}

void VoxelTerrain::set_mesh_block_size(unsigned int mesh_block_size) {
	mesh_block_size = clamp(mesh_block_size, get_data_block_size(), VoxelConstants::MAX_BLOCK_SIZE);

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

	// Unload all mesh blocks regardless of refcount
	_mesh_map.create(po2, 0);

	// Make paired viewers re-view the new meshable area
	for (unsigned int i = 0; i < _paired_viewers.size(); ++i) {
		PairedViewer &viewer = _paired_viewers[i];
		// Resetting both because it's a re-initialization.
		// We could also be doing that before or after their are shifted.
		viewer.state.mesh_box = Box3i();
		viewer.prev_state.mesh_box = Box3i();
	}

	VoxelServer::get_singleton()->set_volume_render_block_size(_volume_id, mesh_block_size);

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

	VoxelServer::get_singleton()->set_volume_data_block_size(_volume_id, 1 << get_data_block_size_pow2());
	VoxelServer::get_singleton()->set_volume_render_block_size(_volume_id, 1 << get_mesh_block_size_pow2());

	// The whole map might change, so regenerate it
	reset_map();

	if ((_stream.is_valid() || _generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	update_configuration_warning();
}

Ref<VoxelMesher> VoxelTerrain::get_mesher() const {
	return _mesher;
}

void VoxelTerrain::set_mesher(Ref<VoxelMesher> mesher) {
	if (mesher == _mesher) {
		return;
	}

	_mesher = mesher;

	stop_updater();

	if (_mesher.is_valid()) {
		start_updater();
		// Voxel appearance might completely change
		remesh_all_blocks();
	}

	update_configuration_warning();
}

Ref<VoxelLibrary> VoxelTerrain::get_voxel_library() const {
	Ref<VoxelMesherBlocky> blocky_mesher = _mesher;
	if (blocky_mesher.is_valid()) {
		return blocky_mesher->get_library();
	}
	return Ref<VoxelLibrary>();
}

void VoxelTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelTerrain::set_collision_layer(int layer) {
	_collision_layer = layer;
	_mesh_map.for_all_blocks([layer](VoxelMeshBlock *block) {
		block->set_collision_layer(layer);
	});
}

int VoxelTerrain::get_collision_layer() const {
	return _collision_layer;
}

void VoxelTerrain::set_collision_mask(int mask) {
	_collision_mask = mask;
	_mesh_map.for_all_blocks([mask](VoxelMeshBlock *block) {
		block->set_collision_mask(mask);
	});
}

int VoxelTerrain::get_collision_mask() const {
	return _collision_mask;
}

void VoxelTerrain::set_collision_margin(float margin) {
	_collision_margin = margin;
	_mesh_map.for_all_blocks([margin](VoxelMeshBlock *block) {
		block->set_collision_margin(margin);
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
}

void VoxelTerrain::set_material(unsigned int id, Ref<Material> material) {
	// TODO Update existing block surfaces
	ERR_FAIL_COND(id < 0 || id >= VoxelMesherBlocky::MAX_MATERIALS);
	_materials[id] = material;
}

Ref<Material> VoxelTerrain::get_material(unsigned int id) const {
	ERR_FAIL_COND_V(id < 0 || id >= VoxelMesherBlocky::MAX_MATERIALS, Ref<Material>());
	return _materials[id];
}

void VoxelTerrain::try_schedule_mesh_update(VoxelMeshBlock *mesh_block) {
	CRASH_COND(mesh_block == nullptr);

	if (mesh_block->get_mesh_state() == VoxelMeshBlock::MESH_UPDATE_NOT_SENT) {
		// Already in the list
		return;
	}
	if (mesh_block->mesh_viewers.get() == 0 && mesh_block->collision_viewers.get() == 0) {
		// No viewers want mesh on this block (why even call this function then?)
		return;
	}

	const int render_to_data_factor = get_mesh_block_size() / get_data_block_size();
	const Box3i bounds_in_data_blocks = _bounds_in_voxels.downscaled(get_data_block_size());
	// Pad by 1 because meshing needs neighbors
	const Box3i data_box = Box3i(mesh_block->position * render_to_data_factor, Vector3i(render_to_data_factor))
								   .padded(1)
								   .clipped(bounds_in_data_blocks);

	// If we get an empty box at this point, something is wrong with the caller
	ERR_FAIL_COND(data_box.is_empty());

	// Check if we have the data
	const bool data_available = data_box.all_cells_match([this](Vector3i bpos) {
		return _data_map.has_block(bpos);
	});

	if (data_available) {
		// Regardless of if the updater is updating the block already,
		// the block could have been modified again so we schedule another update
		mesh_block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_NOT_SENT);
		_blocks_pending_update.push_back(mesh_block->position);
	}
}

void VoxelTerrain::view_data_block(Vector3i bpos) {
	VoxelDataBlock *block = _data_map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
		LoadingBlock *loading_block = _loading_blocks.getptr(bpos);

		if (loading_block == nullptr) {
			// First viewer to request it
			LoadingBlock new_loading_block;
			new_loading_block.viewers.add();

			// Schedule a loading request
			_loading_blocks.set(bpos, new_loading_block);
			_blocks_pending_load.push_back(bpos);

		} else {
			// More viewers
			loading_block->viewers.add();
		}

	} else {
		// The block is loaded
		block->viewers.add();

		// TODO viewers with varying flags during the game is not supported at the moment.
		// They have to be re-created, which may cause world re-load...
	}
}

void VoxelTerrain::view_mesh_block(Vector3i bpos, bool mesh_flag, bool collision_flag) {
	if (mesh_flag == false && collision_flag == false) {
		// Why even call the function?
		return;
	}

	VoxelMeshBlock *block = _mesh_map.get_block(bpos);

	if (block == nullptr) {
		// Create if not found
		block = VoxelMeshBlock::create(bpos, get_mesh_block_size(), 0);
		block->set_world(get_world());
		_mesh_map.set_block(bpos, block);
	}

	if (mesh_flag) {
		block->mesh_viewers.add();
	}
	if (collision_flag) {
		block->collision_viewers.add();
	}

	// This is needed in case a viewer wants to view meshes in places data blocks are already present.
	// Before that, meshes were updated only when a data block was loaded or modified,
	// so changing block size or viewer flags did not make meshes appear.
	try_schedule_mesh_update(block);

	// TODO viewers with varying flags during the game is not supported at the moment.
	// They have to be re-created, which may cause world re-load...
}

void VoxelTerrain::unview_data_block(Vector3i bpos) {
	VoxelDataBlock *block = _data_map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
		LoadingBlock *loading_block = _loading_blocks.getptr(bpos);
		if (loading_block == nullptr) {
			PRINT_VERBOSE("Request to unview a loading block that was never requested");
			// Not expected, but fine I guess
			return;
		}

		loading_block->viewers.remove();

		if (loading_block->viewers.get() == 0) {
			// No longer want to load it
			_loading_blocks.erase(bpos);

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
	VoxelMeshBlock *block = _mesh_map.get_block(bpos);
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

	if (block->collision_viewers.get() == 0 && block->collision_viewers.get() == 0) {
		unload_mesh_block(bpos);
	}
}

namespace {
struct ScheduleSaveAction {
	std::vector<VoxelTerrain::BlockToSave> &blocks_to_save;
	bool with_copy;

	void operator()(VoxelDataBlock *block) {
		// TODO Don't ask for save if the stream doesn't support it!
		if (block->is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelTerrain::BlockToSave b;
			if (with_copy) {
				RWLockRead lock(block->get_voxels().get_lock());
				b.voxels = gd_make_shared<VoxelBufferInternal>();
				block->get_voxels_const().duplicate_to(*b.voxels, true);
			} else {
				b.voxels = block->get_voxels_shared();
			}
			b.position = block->position;
			blocks_to_save.push_back(b);
			block->set_modified(false);
		}
	}
};
} // namespace

void VoxelTerrain::unload_data_block(Vector3i bpos) {
	_data_map.remove_block(bpos, [this, bpos](VoxelDataBlock *block) {
		emit_data_block_unloaded(block);
		// Note: no need to copy the block because it gets removed from the map anyways
		ScheduleSaveAction{ _blocks_to_save, false }(block);
	});

	_loading_blocks.erase(bpos);

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block
}

void VoxelTerrain::unload_mesh_block(Vector3i bpos) {
	_mesh_map.remove_block(bpos, VoxelMeshMap::NoAction());
}

void VoxelTerrain::save_all_modified_blocks(bool with_copy) {
	// That may cause a stutter, so should be used when the player won't notice
	_data_map.for_all_blocks(ScheduleSaveAction{ _blocks_to_save, with_copy });
	// And flush immediately
	send_block_data_requests();
}

const VoxelTerrain::Stats &VoxelTerrain::get_stats() const {
	return _stats;
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
		Ref<VoxelLibrary> library = blocky_mesher->get_library();
		if (library.is_valid()) {
			// TODO Any way to execute this function just after the TRES resource loader has finished to load?
			// VoxelLibrary should be baked ahead of time, like MeshLibrary
			library->bake();
		}
	}

	VoxelServer::get_singleton()->set_volume_mesher(_volume_id, _mesher);
}

void VoxelTerrain::stop_updater() {
	struct ResetMeshStateAction {
		void operator()(VoxelMeshBlock *block) {
			if (block->get_mesh_state() == VoxelMeshBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_NOT_SENT);
			}
		}
	};

	VoxelServer::get_singleton()->invalidate_volume_mesh_requests(_volume_id);
	VoxelServer::get_singleton()->set_volume_mesher(_volume_id, Ref<VoxelMesher>());

	// TODO We can still receive a few mesh delayed mesh updates after this. Is it a problem?
	//_reception_buffers.mesh_output.clear();

	_blocks_pending_update.clear();

	ResetMeshStateAction a;
	_mesh_map.for_all_blocks(a);
}

void VoxelTerrain::remesh_all_blocks() {
	_mesh_map.for_all_blocks([this](VoxelMeshBlock *block) {
		try_schedule_mesh_update(block);
	});
}

void VoxelTerrain::start_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, _stream);
	VoxelServer::get_singleton()->set_volume_generator(_volume_id, _generator);
}

void VoxelTerrain::stop_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, Ref<VoxelStream>());
	VoxelServer::get_singleton()->set_volume_generator(_volume_id, Ref<VoxelGenerator>());
	_loading_blocks.clear();
	_blocks_pending_load.clear();
	_reception_buffers.data_output.clear();
}

void VoxelTerrain::reset_map() {
	// Discard everything, to reload it all

	_data_map.for_all_blocks([this](VoxelDataBlock *block) {
		emit_data_block_unloaded(block);
	});
	_data_map.create(get_data_block_size_pow2(), 0);

	_mesh_map.create(get_mesh_block_size_pow2(), 0);

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
		VoxelMeshBlock *block = _mesh_map.get_block(pos);
		// There isn't necessarily a mesh block, if the edit happens in a boundary,
		// or if it is done next to a viewer that doesn't need meshes
		if (block != nullptr) {
			try_schedule_mesh_update(block);
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
		}
	});

	try_schedule_mesh_update_from_data(box_in_voxels);
}

void VoxelTerrain::_notification(int p_what) {
	struct SetWorldAction {
		World *world;
		SetWorldAction(World *w) :
				world(w) {}
		void operator()(VoxelMeshBlock *block) {
			block->set_world(world);
		}
	};

	struct SetParentVisibilityAction {
		bool visible;
		SetParentVisibilityAction(bool v) :
				visible(v) {}
		void operator()(VoxelMeshBlock *block) {
			block->set_parent_visible(visible);
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
			_mesh_map.for_all_blocks(SetWorldAction(*get_world()));
			break;

		case NOTIFICATION_EXIT_WORLD:
			_mesh_map.for_all_blocks(SetWorldAction(nullptr));
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			_mesh_map.for_all_blocks(SetParentVisibilityAction(is_visible()));
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			const Transform transform = get_global_transform();
			VoxelServer::get_singleton()->set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			_mesh_map.for_all_blocks([&transform](VoxelMeshBlock *block) {
				block->set_parent_transform(transform);
			});

		} break;

		default:
			break;
	}
}

void VoxelTerrain::send_block_data_requests() {
	VOXEL_PROFILE_SCOPE();

	// Blocks to load
	for (size_t i = 0; i < _blocks_pending_load.size(); ++i) {
		const Vector3i block_pos = _blocks_pending_load[i];
		// TODO Batch request
		VoxelServer::get_singleton()->request_block_load(_volume_id, block_pos, 0, false);
	}

	// Blocks to save
	if (_stream.is_valid()) {
		for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
			PRINT_VERBOSE(String("Requesting save of block {0}").format(varray(_blocks_to_save[i].position.to_vec3())));
			const BlockToSave b = _blocks_to_save[i];
			// TODO Batch request
			VoxelServer::get_singleton()->request_voxel_block_save(_volume_id, b.voxels, b.position, 0);
		}
	} else {
		if (_blocks_to_save.size() > 0) {
			PRINT_VERBOSE(String("Not saving {0} blocks because no stream is assigned")
								  .format(varray(SIZE_T_TO_VARIANT(_blocks_to_save.size()))));
		}
	}

	//print_line(String("Sending {0} block requests").format(varray(input.blocks_to_emerge.size())));
	_blocks_pending_load.clear();
	_blocks_to_save.clear();
}

void VoxelTerrain::emit_data_block_loaded(const VoxelDataBlock *block) {
	const Variant vpos = block->position.to_vec3();
	// Not sure about exposing buffers directly... some stuff on them is useful to obtain directly,
	// but also it allows scripters to mess with voxels in a way they should not.
	// Example: modifying voxels without locking them first, while another thread may be reading them at the same time.
	// The same thing could happen the other way around (threaded task modifying voxels while you try to read them).
	// It isn't planned to expose VoxelBuffer locks because there are too many of them,
	// it may likely shift to another system in the future, and might even be changed to no longer inherit Reference.
	// So unless this is absolutely necessary, buffers aren't exposed. Workaround: use VoxelTool
	//const Variant vbuffer = block->voxels;
	//const Variant *args[2] = { &vpos, &vbuffer };
	const Variant *args[1] = { &vpos };
	emit_signal(VoxelStringNames::get_singleton()->block_loaded, args, 1);
}

void VoxelTerrain::emit_data_block_unloaded(const VoxelDataBlock *block) {
	const Variant vpos = block->position.to_vec3();
	// const Variant vbuffer = block->voxels;
	// const Variant *args[2] = { &vpos, &vbuffer };
	const Variant *args[1] = { &vpos };
	emit_signal(VoxelStringNames::get_singleton()->block_unloaded, args, 1);
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

void VoxelTerrain::_process() {
	VOXEL_PROFILE_SCOPE();
	process_viewers();
	process_received_data_blocks();
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
			if (!VoxelServer::get_singleton()->viewer_exists(p.id)) {
				PRINT_VERBOSE("Detected destroyed viewer in VoxelTerrain");
				// Interpret removal as nullified view distance so the same code handling loading of blocks
				// will be used to unload those viewed by this viewer.
				// We'll actually remove unpaired viewers in a second pass.
				p.state.view_distance_voxels = 0;
				unpaired_viewer_indexes.push_back(i);
			}
		}

		const Transform local_to_world_transform = get_global_transform();
		const Transform world_to_local_transform = local_to_world_transform.affine_inverse();

		// Note, this does not support non-uniform scaling
		// TODO There is probably a better way to do this
		const float view_distance_scale = world_to_local_transform.basis.xform(Vector3(1, 0, 0)).length();

		const Box3i bounds_in_data_blocks = _bounds_in_voxels.downscaled(get_data_block_size());
		const Box3i bounds_in_mesh_blocks = _bounds_in_voxels.downscaled(get_mesh_block_size());

		struct UpdatePairedViewer {
			VoxelTerrain &self;
			const Box3i bounds_in_data_blocks;
			const Box3i bounds_in_mesh_blocks;
			const Transform world_to_local_transform;
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

				state.view_distance_voxels = min(view_distance_voxels, self._max_view_distance_voxels);
				state.local_position_voxels = Vector3i::from_floored(local_position);
				state.requires_collisions = VoxelServer::get_singleton()->is_viewer_requiring_collisions(viewer_id);
				state.requires_meshes = VoxelServer::get_singleton()->is_viewer_requiring_visuals(viewer_id);

				// Update data and mesh view boxes

				const int data_block_size = self.get_data_block_size();
				const int mesh_block_size = self.get_mesh_block_size();

				int view_distance_data_blocks;
				Vector3i data_block_pos;

				if (state.requires_meshes || state.requires_collisions) {
					const int view_distance_mesh_blocks = ceildiv(state.view_distance_voxels, mesh_block_size);
					const int render_to_data_factor = (mesh_block_size / data_block_size);
					const Vector3i mesh_block_pos = state.local_position_voxels.floordiv(mesh_block_size);

					// Adding one block of padding because meshing requires neighbors
					view_distance_data_blocks = view_distance_mesh_blocks * render_to_data_factor + 1;

					data_block_pos = mesh_block_pos * render_to_data_factor;
					state.mesh_box = Box3i::from_center_extents(mesh_block_pos, Vector3i(view_distance_mesh_blocks))
											 .clipped(bounds_in_mesh_blocks);

				} else {
					view_distance_data_blocks = ceildiv(state.view_distance_voxels, data_block_size);

					data_block_pos = state.local_position_voxels.floordiv(data_block_size);
					state.mesh_box = Box3i();
				}

				state.data_box = Box3i::from_center_extents(data_block_pos, Vector3i(view_distance_data_blocks))
										 .clipped(bounds_in_data_blocks);
			}
		};

		// New viewers and updates
		UpdatePairedViewer u{
			*this,
			bounds_in_data_blocks,
			bounds_in_mesh_blocks,
			world_to_local_transform,
			view_distance_scale
		};
		VoxelServer::get_singleton()->for_each_viewer(u);
	}

	const bool stream_enabled = (_stream.is_valid() || _generator.is_valid()) &&
								(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor);

	// Find out which blocks need to appear and which need to be unloaded
	if (stream_enabled) {
		VOXEL_PROFILE_SCOPE();

		for (size_t i = 0; i < _paired_viewers.size(); ++i) {
			const PairedViewer &viewer = _paired_viewers[i];

			{
				const Box3i &new_data_box = viewer.state.data_box;
				const Box3i &prev_data_box = viewer.prev_state.data_box;

				if (prev_data_box != new_data_box) {
					VOXEL_PROFILE_SCOPE();

					// Unview blocks that just fell out of range
					prev_data_box.difference(new_data_box, [this, &viewer](Box3i out_of_range_box) {
						out_of_range_box.for_each_cell([this, &viewer](Vector3i bpos) {
							unview_data_block(bpos);
						});
					});

					// View blocks that just entered the range
					new_data_box.difference(prev_data_box, [this, &viewer](Box3i box_to_load) {
						box_to_load.for_each_cell([this, &viewer](Vector3i bpos) {
							// Load or update block
							view_data_block(bpos);
						});
					});
				}
			}

			{
				const Box3i &new_mesh_box = viewer.state.mesh_box;
				const Box3i &prev_mesh_box = viewer.prev_state.mesh_box;

				if (prev_mesh_box != new_mesh_box) {
					VOXEL_PROFILE_SCOPE();

					// Unview blocks that just fell out of range
					prev_mesh_box.difference(new_mesh_box, [this, &viewer](Box3i out_of_range_box) {
						out_of_range_box.for_each_cell([this, &viewer](Vector3i bpos) {
							unview_mesh_block(bpos,
									viewer.prev_state.requires_meshes,
									viewer.prev_state.requires_collisions);
						});
					});

					// View blocks that just entered the range
					new_mesh_box.difference(prev_mesh_box, [this, &viewer](Box3i box_to_load) {
						box_to_load.for_each_cell([this, &viewer](Vector3i bpos) {
							// Load or update block
							view_mesh_block(bpos,
									viewer.state.requires_meshes,
									viewer.state.requires_collisions);
						});
					});
				}

				// Blocks that remained within range of the viewer may need some changes too if viewer flags were modified.
				// This operates on a DISTINCT set of blocks than the one above.

				if (viewer.state.requires_collisions != viewer.prev_state.requires_collisions) {
					const Box3i box = new_mesh_box.clipped(prev_mesh_box);
					if (viewer.state.requires_collisions) {
						box.for_each_cell([this](Vector3i bpos) {
							view_mesh_block(bpos, false, true);
						});

					} else {
						box.for_each_cell([this](Vector3i bpos) {
							unview_mesh_block(bpos, false, true);
						});
					}
				}

				if (viewer.state.requires_meshes != viewer.prev_state.requires_meshes) {
					const Box3i box = new_mesh_box.clipped(prev_mesh_box);
					if (viewer.state.requires_meshes) {
						box.for_each_cell([this](Vector3i bpos) {
							view_mesh_block(bpos, true, false);
						});

					} else {
						box.for_each_cell([this](Vector3i bpos) {
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
		PRINT_VERBOSE("Unpairing viewer from VoxelTerrain");
		// Iterating backward so indexes of paired viewers will not change because of the removal
		const size_t vi = unpaired_viewer_indexes[unpaired_viewer_indexes.size() - i - 1];
		_paired_viewers[vi] = _paired_viewers.back();
		_paired_viewers.pop_back();
	}

	// It's possible the user didn't set a stream yet, or it is turned off
	if (stream_enabled) {
		send_block_data_requests();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();
}

void VoxelTerrain::process_received_data_blocks() {
	ProfilingClock profiling_clock;

	_stats.dropped_block_loads = 0;

	const bool stream_enabled = (_stream.is_valid() || _generator.is_valid()) &&
								(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor);

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters. It should only happen on first load, though.
	{
		VOXEL_PROFILE_SCOPE();

		//print_line(String("Receiving {0} blocks").format(varray(output.emerged_blocks.size())));
		for (size_t i = 0; i < _reception_buffers.data_output.size(); ++i) {
			VoxelServer::BlockDataOutput &ob = _reception_buffers.data_output[i];

			if (ob.type == VoxelServer::BlockDataOutput::TYPE_SAVE) {
				if (ob.dropped) {
					ERR_PRINT(String("Could not save block {0}").format(varray(ob.position.to_vec3())));
				}
				continue;
			}

			CRASH_COND(ob.type != VoxelServer::BlockDataOutput::TYPE_LOAD);

			const Vector3i block_pos = ob.position;

			LoadingBlock loading_block;
			{
				LoadingBlock *loading_block_ptr = _loading_blocks.getptr(block_pos);

				if (loading_block_ptr == nullptr) {
					// That block was not requested or is no longer needed, drop it.
					++_stats.dropped_block_loads;
					continue;
				}

				loading_block = *loading_block_ptr;
			}

			if (ob.dropped) {
				// That block was cancelled by the server, but we are still expecting it.
				// We'll have to request it again.
				PRINT_VERBOSE(String("Received a block loading drop while we were still expecting it: "
									 "lod{0} ({1}, {2}, {3}), re-requesting it")
									  .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));
				++_stats.dropped_block_loads;

				_blocks_pending_load.push_back(ob.position);
				continue;
			}

			// Now we got the block. If we still have to drop it, the cause will be an error.
			_loading_blocks.erase(block_pos);

			CRASH_COND(ob.voxels == nullptr);

			const Vector3i expected_block_size(_data_map.get_block_size());
			if (ob.voxels->get_size() != expected_block_size) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT(String("Block size obtained from stream is different from expected size. "
								 "Expected {0}, got {1}")
								  .format(varray(expected_block_size.to_vec3(), ob.voxels->get_size().to_vec3())));
				++_stats.dropped_block_loads;
				continue;
			}

			// Create or update block data
			VoxelDataBlock *block = _data_map.get_block(block_pos);
			const bool was_not_loaded = block == nullptr;
			block = _data_map.set_block_buffer(block_pos, ob.voxels);

			if (was_not_loaded) {
				// Set viewers count that are currently expecting the block
				block->viewers = loading_block.viewers;
			}

			emit_data_block_loaded(block);

			// The block itself might not be suitable for meshing yet, but blocks surrounding it might be now
			{
				VOXEL_PROFILE_SCOPE();
				try_schedule_mesh_update_from_data(
						Box3i(_data_map.block_to_voxel(block_pos), Vector3i(get_data_block_size())));
			}
		}

		_reception_buffers.data_output.clear();

		// We might have requested some blocks again (if we got a dropped one while we still need them)
		if (stream_enabled) {
			send_block_data_requests();
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();
}

void VoxelTerrain::process_meshing() {
	ProfilingClock profiling_clock;

	_stats.dropped_block_meshs = 0;

	// Send mesh updates
	{
		VOXEL_PROFILE_SCOPE();

		//const int used_channels_mask = get_used_channels_mask();
		const int mesh_to_data_factor = get_mesh_block_size() / get_data_block_size();

		for (size_t bi = 0; bi < _blocks_pending_update.size(); ++bi) {
			const Vector3i mesh_block_pos = _blocks_pending_update[bi];

			VoxelMeshBlock *mesh_block = _mesh_map.get_block(mesh_block_pos);

			// If we got here, it must have been because of scheduling an update
			ERR_CONTINUE(mesh_block == nullptr);
			ERR_CONTINUE(mesh_block->get_mesh_state() != VoxelMeshBlock::MESH_UPDATE_NOT_SENT);

			// Pad by 1 because meshing requires neighbors
			const Box3i data_box =
					Box3i(mesh_block_pos * mesh_to_data_factor, Vector3i(mesh_to_data_factor)).padded(1);

#ifdef DEBUG_ENABLED
			// We must have picked up a valid data block
			{
				const Vector3i anchor_pos = data_box.pos + Vector3i(1, 1, 1);
				const VoxelDataBlock *data_block = _data_map.get_block(anchor_pos);
				ERR_CONTINUE(data_block == nullptr);
			}
#endif

			VoxelServer::BlockMeshInput mesh_request;
			mesh_request.render_block_position = mesh_block_pos;
			mesh_request.lod = 0;
			//mesh_request.data_blocks_count = data_box.size.volume();

			// This iteration order is specifically chosen to match VoxelServer and threaded access
			data_box.for_each_cell_zxy([this, &mesh_request](Vector3i data_block_pos) {
				VoxelDataBlock *data_block = _data_map.get_block(data_block_pos);
				if (data_block != nullptr) {
					mesh_request.data_blocks[mesh_request.data_blocks_count] = data_block->get_voxels_shared();
				}
				++mesh_request.data_blocks_count;
			});

#ifdef DEBUG_ENABLED
			{
				unsigned int count = 0;
				for (unsigned int i = 0; i < mesh_request.data_blocks_count; ++i) {
					if (mesh_request.data_blocks[i] != nullptr) {
						++count;
					}
				}
				// Blocks that were in the list must have been scheduled because we have data for them!
				ERR_CONTINUE(count == 0);
			}
#endif

			//print_line(String("DDD request {0}").format(varray(mesh_request.render_block_position.to_vec3())));
			VoxelServer::get_singleton()->request_block_mesh(_volume_id, mesh_request);

			mesh_block->set_mesh_state(VoxelMeshBlock::MESH_UPDATE_SENT);
		}

		_blocks_pending_update.clear();
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	//print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") + String::num(_block_update_queue.size()));
}

void VoxelTerrain::apply_mesh_update(const VoxelServer::BlockMeshOutput &ob) {
	VOXEL_PROFILE_SCOPE();
	//print_line(String("DDD receive {0}").format(varray(ob.position.to_vec3())));

	VoxelMeshBlock *block = _mesh_map.get_block(ob.position);
	if (block == nullptr) {
		//print_line("- no longer loaded");
		// That block is no longer loaded, drop the result
		++_stats.dropped_block_meshs;
		return;
	}

	if (ob.type == VoxelServer::BlockMeshOutput::TYPE_DROPPED) {
		// That block is loaded, but its meshing request was dropped.
		// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
		PRINT_VERBOSE("Received a block mesh drop while we were still expecting it");
		++_stats.dropped_block_meshs;
		return;
	}

	Ref<ArrayMesh> mesh;
	mesh.instance();

	Vector<Array> collidable_surfaces; //need to put both blocky and smooth surfaces into one list

	int surface_index = 0;
	for (int i = 0; i < ob.surfaces.surfaces.size(); ++i) {
		Array surface = ob.surfaces.surfaces[i];
		if (surface.empty()) {
			continue;
		}

		CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(surface)) {
			continue;
		}

		collidable_surfaces.push_back(surface);

		mesh->add_surface_from_arrays(
				ob.surfaces.primitive_type, surface, Array(), ob.surfaces.compression_flags);
		mesh->surface_set_material(surface_index, _materials[i]);
		++surface_index;
	}

	if (is_mesh_empty(mesh)) {
		mesh = Ref<Mesh>();
		collidable_surfaces.clear();
	}

	const bool gen_collisions = _generate_collisions && block->collision_viewers.get() > 0;

	block->set_mesh(mesh);
	if (gen_collisions) {
		block->set_collision_mesh(collidable_surfaces, get_tree()->is_debugging_collisions_hint(), this,
				_collision_margin);
		block->set_collision_layer(_collision_layer);
		block->set_collision_mask(_collision_mask);
	}
	block->set_visible(true);
	block->set_parent_visible(is_visible());
	block->set_parent_transform(get_global_transform());
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
	_bounds_in_voxels = box.clipped(
			Box3i::from_center_extents(Vector3i(), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT)));

	// Round to block size
	_bounds_in_voxels = _bounds_in_voxels.snapped(get_data_block_size());

	const unsigned int largest_dimension = static_cast<unsigned int>(max(max(box.size.x, box.size.y), box.size.z));
	if (largest_dimension > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
		// Cap view distance to make sure you don't accidentally blow up memory when changing parameters
		if (_max_view_distance_voxels > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
			_max_view_distance_voxels = min(_max_view_distance_voxels, MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME);
			_change_notify();
		}
	}
	// TODO Editor gizmo bounds
}

Box3i VoxelTerrain::get_bounds() const {
	return _bounds_in_voxels;
}

Vector3 VoxelTerrain::_b_voxel_to_data_block(Vector3 pos) const {
	return Vector3i(_data_map.voxel_to_block(Vector3i::from_floored(pos))).to_vec3();
}

Vector3 VoxelTerrain::_b_data_block_to_voxel(Vector3 pos) const {
	return Vector3i(_data_map.block_to_voxel(Vector3i::from_floored(pos))).to_vec3();
}

void VoxelTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

// Explicitely ask to save a block if it was modified
void VoxelTerrain::_b_save_block(Vector3 p_block_pos) {
	const Vector3i block_pos(Vector3i::from_floored(p_block_pos));

	VoxelDataBlock *block = _data_map.get_block(block_pos);
	ERR_FAIL_COND(block == nullptr);

	if (!block->is_modified()) {
		return;
	}

	ScheduleSaveAction{ _blocks_to_save, true }(block);
}

void VoxelTerrain::_b_set_bounds(AABB aabb) {
	ERR_FAIL_COND(!is_valid_size(aabb.size));
	set_bounds(Box3i(Vector3i::from_rounded(aabb.position), Vector3i::from_rounded(aabb.size)));
}

AABB VoxelTerrain::_b_get_bounds() const {
	const Box3i b = get_bounds();
	return AABB(b.pos.to_vec3(), b.size.to_vec3());
}

void VoxelTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_material", "id", "material"), &VoxelTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material", "id"), &VoxelTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_max_view_distance", "distance_in_voxels"), &VoxelTerrain::set_max_view_distance);
	ClassDB::bind_method(D_METHOD("get_max_view_distance"), &VoxelTerrain::get_max_view_distance);

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

	// TODO Rename `_voxel_bounds`
	ClassDB::bind_method(D_METHOD("set_bounds"), &VoxelTerrain::_b_set_bounds);
	ClassDB::bind_method(D_METHOD("get_bounds"), &VoxelTerrain::_b_get_bounds);

	//ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelTerrain::_on_stream_params_changed);

	ADD_GROUP("Bounds", "");

	ADD_PROPERTY(PropertyInfo(Variant::AABB, "bounds"), "set_bounds", "get_bounds");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_view_distance"), "set_max_view_distance", "get_max_view_distance");

	ADD_GROUP("Collisions", "");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"),
			"set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "collision_margin"), "set_collision_margin", "get_collision_margin");

	ADD_GROUP("Advanced", "");

	// TODO Should probably be in the parent class?
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"),
			"set_run_stream_in_editor", "is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_block_size"), "set_mesh_block_size", "get_mesh_block_size");

	// TODO Add back access to block, but with an API securing multithreaded access
	ADD_SIGNAL(MethodInfo(VoxelStringNames::get_singleton()->block_loaded,
			PropertyInfo(Variant::VECTOR3, "position")));
	ADD_SIGNAL(MethodInfo(VoxelStringNames::get_singleton()->block_unloaded,
			PropertyInfo(Variant::VECTOR3, "position")));
}
