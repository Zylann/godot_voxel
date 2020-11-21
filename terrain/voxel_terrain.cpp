#include "voxel_terrain.h"
#include "../edition/voxel_tool_terrain.h"
#include "../server/voxel_server.h"
#include "../streams/voxel_stream_file.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/profiling_clock.h"
#include "../util/utility.h"
#include "../voxel_constants.h"
#include "../voxel_string_names.h"
#include "voxel_block.h"
#include "voxel_map.h"

#include <core/core_string_names.h>
#include <core/engine.h>
#include <scene/3d/mesh_instance.h>

VoxelTerrain::VoxelTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	set_notify_transform(true);

	// Infinite by default
	_bounds_in_voxels = Rect3i::from_center_extents(Vector3i(0), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT));

	_volume_id = VoxelServer::get_singleton()->add_volume(&_reception_buffers, VoxelServer::VOLUME_SPARSE_GRID);

	Ref<VoxelLibrary> library;
	library.instance();
	set_voxel_library(library);
}

VoxelTerrain::~VoxelTerrain() {
	PRINT_VERBOSE("Destroying VoxelTerrain");

	// Schedule saving of all modified blocks,
	// without copy because we are destroying the map anyways
	save_all_modified_blocks(false);

	VoxelServer::get_singleton()->remove_volume(_volume_id);
}

String VoxelTerrain::get_configuration_warning() const {
	if (_stream.is_valid()) {
		Ref<Script> script = _stream->get_script();
		if (script.is_valid()) {
			if (script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this properly?
				return TTR("Be careful! Don't edit your custom stream while it's running, "
						   "it can cause crashes. Turn off `run_stream_in_editor` before doing so.");
			} else {
				return TTR("The custom stream is not tool, the editor won't be able to use it.");
			}
		}
		if (!(_stream->get_used_channels_mask() & ((1 << VoxelBuffer::CHANNEL_TYPE) | (1 << VoxelBuffer::CHANNEL_SDF)))) {
			return TTR("VoxelTerrain supports only stream channels \"Type\" or \"Sdf\", "
					   "but `get_used_channels_mask()` tells it's providing none of these.");
		}
	}
	return String();
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
			if (_stream->has_script()) {
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

void VoxelTerrain::set_block_size_po2(unsigned int p_block_size_po2) {
	ERR_FAIL_COND(p_block_size_po2 < 1);
	ERR_FAIL_COND(p_block_size_po2 > 32);

	unsigned int block_size_po2 = p_block_size_po2;
	Ref<VoxelStreamFile> file_stream = _stream;
	if (file_stream.is_valid()) {
		block_size_po2 = file_stream->get_block_size_po2();
	}

	if (block_size_po2 == get_block_size_pow2()) {
		return;
	}

	_on_stream_params_changed();
}

void VoxelTerrain::_set_block_size_po2(int p_block_size_po2) {
	_map.create(p_block_size_po2, 0);
}

unsigned int VoxelTerrain::get_block_size_pow2() const {
	return _map.get_block_size_pow2();
}

void VoxelTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	Ref<VoxelStreamFile> file_stream = _stream;
	if (file_stream.is_valid()) {
		int stream_block_size_po2 = file_stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);
	}

	VoxelServer::get_singleton()->set_volume_block_size(_volume_id, 1 << get_block_size_pow2());

	// The whole map might change, so regenerate it
	reset_map();

	if (_stream.is_valid() && (Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	update_configuration_warning();
}

Ref<VoxelLibrary> VoxelTerrain::get_voxel_library() const {
	return _library;
}

void VoxelTerrain::set_voxel_library(Ref<VoxelLibrary> library) {
	if (library == _library) {
		return;
	}

#ifdef TOOLS_ENABLED
	if (library->get_voxel_count() == 0) {
		library->load_default();
	}
#endif

	_library = library;

	stop_updater();
	start_updater();

	// Voxel appearance might completely change
	make_all_view_dirty();
}

void VoxelTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

unsigned int VoxelTerrain::get_max_view_distance() const {
	return _max_view_distance_blocks * _map.get_block_size();
}

void VoxelTerrain::set_max_view_distance(unsigned int distance_in_voxels) {
	ERR_FAIL_COND(distance_in_voxels < 0);
	const unsigned int d = distance_in_voxels / _map.get_block_size();
	if (d != _max_view_distance_blocks) {
		PRINT_VERBOSE(String("View distance changed from ") +
					  String::num(_max_view_distance_blocks) + String(" blocks to ") + String::num(d));
		_max_view_distance_blocks = d;
		// Blocks too far away will be removed in _process, same for blocks to load
	}
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

void VoxelTerrain::make_block_dirty(Vector3i bpos) {
	VoxelBlock *block = _map.get_block(bpos);
	ERR_FAIL_COND_MSG(block == nullptr, "Requested update to a block that isn't loaded");
	make_block_dirty(block);
}

void VoxelTerrain::make_block_dirty(VoxelBlock *block) {
	// TODO Immediate update viewer distance?
	CRASH_COND(block == nullptr);
	block->set_modified(true);
	try_schedule_block_update(block);

	//OS::get_singleton()->print("Dirty (%i, %i, %i)", bpos.x, bpos.y, bpos.z);

	// TODO What if a block is made dirty, goes through threaded update, then gets changed again before it gets updated?
	// this will make the second change ignored, which is not correct!
}

void VoxelTerrain::try_schedule_block_update(VoxelBlock *block) {
	CRASH_COND(block == nullptr);

	// If no update was requested already, and if there are viewers requiring a mesh
	if (block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT &&
			(block->viewers.get(VoxelViewerRefCount::TYPE_MESH) != 0 ||
					block->viewers.get(VoxelViewerRefCount::TYPE_COLLISION) != 0)) {
		// Regardless of if the updater is updating the block already,
		// the block was modified again so we schedule another update
		block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
		_blocks_pending_update.push_back(block->position);
	}
}

void VoxelTerrain::view_block(Vector3i bpos, bool data_flag, bool mesh_flag, bool collision_flag) {
	VoxelBlock *block = _map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
		LoadingBlock *loading_block = _loading_blocks.getptr(bpos);

		if (loading_block == nullptr) {
			// First viewer to request it
			LoadingBlock new_loading_block;
			new_loading_block.viewers.add(data_flag, mesh_flag, collision_flag);

			// Schedule a loading request
			_loading_blocks.set(bpos, new_loading_block);
			_blocks_pending_load.push_back(bpos);

		} else {
			// More viewers
			loading_block->viewers.add(data_flag, mesh_flag, collision_flag);
		}

	} else {
		// The block is loaded
		VoxelViewerRefCount &viewers = block->viewers;

		if (data_flag) {
			viewers.add(VoxelViewerRefCount::TYPE_DATA);
		}

		if (mesh_flag) {
			viewers.add(VoxelViewerRefCount::TYPE_MESH);
			if (viewers.get(VoxelViewerRefCount::TYPE_MESH) == 1) {
				// First to request a mesh (means it was not requested when the block was loaded earlier)
				// Trigger mesh update
				try_schedule_block_update(block);
			}
		}

		if (collision_flag) {
			viewers.add(VoxelViewerRefCount::TYPE_COLLISION);
			if (viewers.get(VoxelViewerRefCount::TYPE_COLLISION) == 1) {
				try_schedule_block_update(block);
			}
		}

		// TODO viewers with varying flags during the game is not supported at the moment.
		// They have to be re-created, which may cause world re-load...
	}
}

void VoxelTerrain::unview_block(Vector3i bpos, bool data_flag, bool mesh_flag, bool collision_flag) {
	VoxelBlock *block = _map.get_block(bpos);

	if (block == nullptr) {
		// The block isn't loaded
		LoadingBlock *loading_block = _loading_blocks.getptr(bpos);
		if (loading_block == nullptr) {
			PRINT_VERBOSE("Request to unview a loading block that was never requested");
			// Not expected, but fine I guess
			return;
		}

		loading_block->viewers.remove(data_flag, mesh_flag, collision_flag);

		if (loading_block->viewers.get(VoxelViewerRefCount::TYPE_DATA) == 0) {
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
		VoxelViewerRefCount &viewers = block->viewers;

		if (mesh_flag) {
			viewers.remove(VoxelViewerRefCount::TYPE_MESH);
			if (viewers.get(VoxelViewerRefCount::TYPE_MESH) == 0) {
				// Mesh no longer required
				block->drop_mesh();
			}
		}

		if (collision_flag) {
			viewers.remove(VoxelViewerRefCount::TYPE_COLLISION);
			if (viewers.get(VoxelViewerRefCount::TYPE_COLLISION) == 0) {
				// Collision no longer required
				block->drop_collision();
			}
		}

		if (data_flag) {
			viewers.remove(VoxelViewerRefCount::TYPE_DATA);
		}
		if (viewers.get(VoxelViewerRefCount::TYPE_DATA) == 0) {
			// The block itself is no longer wanted
			immerge_block(bpos);
		}
	}
}

namespace {
struct ScheduleSaveAction {
	std::vector<VoxelTerrain::BlockToSave> &blocks_to_save;
	bool with_copy;

	void operator()(VoxelBlock *block) {
		// TODO Don't ask for save if the stream doesn't support it!
		if (block->is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelTerrain::BlockToSave b;
			if (with_copy) {
				RWLockRead lock(block->voxels->get_lock());
				b.voxels = block->voxels->duplicate(true);
			} else {
				b.voxels = block->voxels;
			}
			b.position = block->position;
			blocks_to_save.push_back(b);
			block->set_modified(false);
		}
	}
};
} // namespace

void VoxelTerrain::immerge_block(Vector3i bpos) {
	_map.remove_block(bpos, [this, bpos](VoxelBlock *block) {
		emit_block_unloaded(block);
		// Note: no need to copy the block because it gets removed from the map anyways
		ScheduleSaveAction{ _blocks_to_save, false }(block);
	});

	_loading_blocks.erase(bpos);

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block

	// for (size_t i = 0; i < _blocks_pending_update.size(); ++i) {
	// 	if (_blocks_pending_update[i] == bpos) {
	// 		_blocks_pending_update[i] = _blocks_pending_update.back();
	// 		_blocks_pending_update.pop_back();
	// 		break;
	// 	}
	// }
}

void VoxelTerrain::save_all_modified_blocks(bool with_copy) {
	// That may cause a stutter, so should be used when the player won't notice
	_map.for_all_blocks(ScheduleSaveAction{ _blocks_to_save, with_copy });
	// And flush immediately
	send_block_data_requests();
}

Dictionary VoxelTerrain::get_statistics() const {
	Dictionary d;

	// Breakdown of time spent in _process
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_request_blocks_to_load"] = _stats.time_request_blocks_to_load;
	d["time_process_load_responses"] = _stats.time_process_load_responses;
	d["time_request_blocks_to_update"] = _stats.time_request_blocks_to_update;
	d["time_process_update_responses"] = _stats.time_process_update_responses;

	d["remaining_main_thread_blocks"] = (int)_reception_buffers.mesh_output.size();
	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;
	d["updated_blocks"] = _stats.updated_blocks;

	return d;
}

//void VoxelTerrain::make_blocks_dirty(Vector3i min, Vector3i size) {
//	Vector3i max = min + size;
//	Vector3i pos;
//	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
//		for (pos.y = min.y; pos.y < max.y; ++pos.y) {
//			for (pos.x = min.x; pos.x < max.x; ++pos.x) {
//				make_block_dirty(pos);
//			}
//		}
//	}
//}

void VoxelTerrain::make_all_view_dirty() {
	// Mark all loaded blocks dirty within range of viewers that require meshes
	_map.for_all_blocks([this](VoxelBlock *b) {
		if (b->viewers.get(VoxelViewerRefCount::TYPE_MESH) > 0) {
			make_block_dirty(b);
		}
	});

	//	Vector3i radius(_view_distance_blocks, _view_distance_blocks, _view_distance_blocks);
	//	make_blocks_dirty(-radius, 2*radius);
}

void VoxelTerrain::start_updater() {
	if (_library.is_valid()) {
		// TODO Any way to execute this function just after the TRES resource loader has finished to load?
		// VoxelLibrary should be baked ahead of time, like MeshLibrary
		_library->bake();
	}

	VoxelServer::get_singleton()->set_volume_voxel_library(_volume_id, _library);
}

void VoxelTerrain::stop_updater() {
	struct ResetMeshStateAction {
		void operator()(VoxelBlock *block) {
			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
			}
		}
	};

	VoxelServer::get_singleton()->invalidate_volume_mesh_requests(_volume_id);
	VoxelServer::get_singleton()->set_volume_voxel_library(_volume_id, Ref<VoxelLibrary>());

	_reception_buffers.mesh_output.clear();
	_blocks_pending_update.clear();

	ResetMeshStateAction a;
	_map.for_all_blocks(a);
}

void VoxelTerrain::start_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, _stream);
}

void VoxelTerrain::stop_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, Ref<VoxelStream>());
	_loading_blocks.clear();
	_blocks_pending_load.clear();
	_reception_buffers.data_output.clear();
}

void VoxelTerrain::reset_map() {
	// Discard everything, to reload it all

	_map.for_all_blocks([this](VoxelBlock *block) {
		emit_block_unloaded(block);
	});
	_map.create(get_block_size_pow2(), 0);

	_loading_blocks.clear();
	_blocks_pending_load.clear();
	_blocks_pending_update.clear();
	_blocks_to_save.clear();

	// No need to care about refcounts, we drop everything anyways. Will pair it back on next process.
	_paired_viewers.clear();
}

inline int get_border_index(int x, int max) {
	return x == 0 ? 0 : x != max ? 1 : 2;
}

void VoxelTerrain::make_voxel_dirty(Vector3i pos) {
	if (!_bounds_in_voxels.contains(pos)) {
		PRINT_VERBOSE(String("Voxel {0} can't be made dirty out of volume bounds {1}")
							  .format(varray(_bounds_in_voxels.to_string(), pos.to_vec3())));
		return;
	}

	// Update the block in which the voxel is
	const Vector3i bpos = _map.voxel_to_block(pos);
	make_block_dirty(bpos);
	//OS::get_singleton()->print("Dirty (%i, %i, %i)\n", bpos.x, bpos.y, bpos.z);

	// Update neighbor blocks if the voxel is touching a boundary

	const Vector3i rpos = _map.to_local(pos);

	// TODO Thread-safe way of getting this parameter
	const bool check_corners = true; //_mesher->get_occlusion_enabled();

	const int max = _map.get_block_size() - 1;

	if (rpos.x == 0) {
		make_block_dirty(bpos - Vector3i(1, 0, 0));
	} else if (rpos.x == max) {
		make_block_dirty(bpos + Vector3i(1, 0, 0));
	}

	if (rpos.y == 0) {
		make_block_dirty(bpos - Vector3i(0, 1, 0));
	} else if (rpos.y == max) {
		make_block_dirty(bpos + Vector3i(0, 1, 0));
	}

	if (rpos.z == 0) {
		make_block_dirty(bpos - Vector3i(0, 0, 1));
	} else if (rpos.z == max) {
		make_block_dirty(bpos + Vector3i(0, 0, 1));
	}

	// We might want to update blocks in corners in order to update ambient occlusion
	if (check_corners) {

		//       24------25------26
		//       /|              /|
		//      / |             / |
		//    21  |           23  |
		//    /  15           /  17
		//   /    |          /    |
		// 18------19------20     |
		//  |     |         |     |
		//  |     6-------7-|-----8
		//  |    /          |    /
		//  9   /          11   /
		//  |  3            |  5
		//  | /             | /      y z
		//  |/              |/       |/
		//  0-------1-------2        o--x

		// I'm not good at writing piles of ifs

		static const int normals[27][3] = {
			{ -1, -1, -1 }, { 0, -1, -1 }, { 1, -1, -1 },
			{ -1, -1, 0 }, { 0, -1, 0 }, { 1, -1, 0 },
			{ -1, -1, 1 }, { 0, -1, 1 }, { 1, -1, 1 },

			{ -1, 0, -1 }, { 0, 0, -1 }, { 1, 0, -1 },
			{ -1, 0, 0 }, { 0, 0, 0 }, { 1, 0, 0 },
			{ -1, 0, 1 }, { 0, 0, 1 }, { 1, 0, 1 },

			{ -1, 1, -1 }, { 0, 1, -1 }, { 1, 1, -1 },
			{ -1, 1, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
			{ -1, 1, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
		};
		static const int ce_counts[27] = {
			4, 1, 4,
			1, 0, 1,
			4, 1, 4,

			1, 0, 1,
			0, 0, 0,
			1, 0, 1,

			4, 1, 4,
			1, 0, 1,
			4, 1, 4
		};
		static const int ce_indexes_lut[27][4] = {
			{ 0, 1, 3, 9 }, { 1 }, { 2, 1, 5, 11 },
			{ 3 }, {}, { 5 },
			{ 6, 3, 7, 15 }, { 7 }, { 8, 7, 5, 17 },

			{ 9 }, {}, { 11 },
			{}, {}, {},
			{ 15 }, {}, { 17 },

			{ 18, 9, 19, 21 }, { 19 }, { 20, 11, 19, 23 },
			{ 21 }, {}, { 23 },
			{ 24, 15, 21, 25 }, { 25 }, { 26, 17, 23, 25 }
		};

		const int m = get_border_index(rpos.x, max) + 3 * get_border_index(rpos.z, max) + 9 * get_border_index(rpos.y, max);

		const int *ce_indexes = ce_indexes_lut[m];
		const int ce_count = ce_counts[m];
		//OS::get_singleton()->print("m=%i, rpos=(%i, %i, %i)\n", m, rpos.x, rpos.y, rpos.z);

		for (int i = 0; i < ce_count; ++i) {
			// TODO Because it's about ambient occlusion across 1 voxel only,
			// we could optimize it even more by looking at neighbor voxels,
			// and discard the update if we know it won't change anything
			const int *normal = normals[ce_indexes[i]];
			const Vector3i nbpos(bpos.x + normal[0], bpos.y + normal[1], bpos.z + normal[2]);
			//OS::get_singleton()->print("Corner dirty (%i, %i, %i)\n", nbpos.x, nbpos.y, nbpos.z);
			make_block_dirty(nbpos);
		}
	}
}

void VoxelTerrain::make_area_dirty(Rect3i box) {
	box.clip(_bounds_in_voxels);

	Vector3i min_pos = box.pos;
	Vector3i max_pos = box.pos + box.size - Vector3(1, 1, 1);

	// TODO Thread-safe way of getting this parameter
	const bool check_corners = true; //_mesher->get_occlusion_enabled();

	if (check_corners) {
		min_pos -= Vector3i(1, 1, 1);
		max_pos += Vector3i(1, 1, 1);

	} else {
		Vector3i min_rpos = _map.to_local(min_pos);
		if (min_rpos.x == 0) {
			--min_pos.x;
		}
		if (min_rpos.y == 0) {
			--min_pos.y;
		}
		if (min_rpos.z == 0) {
			--min_pos.z;
		}

		const int max = _map.get_block_size() - 1;
		const Vector3i max_rpos = _map.to_local(max_pos);
		if (max_rpos.x == max) {
			++max_pos.x;
		}
		if (max_rpos.y == max) {
			++max_pos.y;
		}
		if (max_rpos.z == max) {
			++max_pos.z;
		}
	}

	const Vector3i min_block_pos = _map.voxel_to_block(min_pos);
	const Vector3i max_block_pos = _map.voxel_to_block(max_pos);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z <= max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x <= max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y <= max_block_pos.y; ++bpos.y) {
				make_block_dirty(bpos);
			}
		}
	}
}

void VoxelTerrain::_notification(int p_what) {
	struct SetWorldAction {
		World *world;
		SetWorldAction(World *w) :
				world(w) {}
		void operator()(VoxelBlock *block) {
			block->set_world(world);
		}
	};

	struct SetParentVisibilityAction {
		bool visible;
		SetParentVisibilityAction(bool v) :
				visible(v) {}
		void operator()(VoxelBlock *block) {
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

		case NOTIFICATION_ENTER_WORLD: {
			_map.for_all_blocks(SetWorldAction(*get_world()));
		} break;

		case NOTIFICATION_EXIT_WORLD:
			_map.for_all_blocks(SetWorldAction(nullptr));
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			_map.for_all_blocks(SetParentVisibilityAction(is_visible()));
			break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			const Transform transform = get_global_transform();
			VoxelServer::get_singleton()->set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			_map.for_all_blocks([&transform](VoxelBlock *block) {
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
		VoxelServer::get_singleton()->request_block_load(_volume_id, block_pos, 0);
	}

	// Blocks to save
	for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
		PRINT_VERBOSE(String("Requesting save of block {0}").format(varray(_blocks_to_save[i].position.to_vec3())));
		const BlockToSave b = _blocks_to_save[i];
		// TODO Batch request
		VoxelServer::get_singleton()->request_block_save(_volume_id, b.voxels, b.position, 0);
	}

	//print_line(String("Sending {0} block requests").format(varray(input.blocks_to_emerge.size())));
	_blocks_pending_load.clear();
	_blocks_to_save.clear();
}

void VoxelTerrain::emit_block_loaded(const VoxelBlock *block) {
	const Variant vpos = block->position.to_vec3();
	const Variant vbuffer = block->voxels;
	const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton()->block_loaded, args, 2);
}

void VoxelTerrain::emit_block_unloaded(const VoxelBlock *block) {
	const Variant vpos = block->position.to_vec3();
	const Variant vbuffer = block->voxels;
	const Variant *args[2] = { &vpos, &vbuffer };
	emit_signal(VoxelStringNames::get_singleton()->block_unloaded, args, 2);
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

	// TODO Should be able to run without library, tho!
	if (_library.is_null()) {
		return;
	}
	// print_line(String("D:{0} M:{1}")
	// 				   .format(varray(_reception_buffers.data_output.size(), _reception_buffers.mesh_output.size())));

	ProfilingClock profiling_clock;

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;

	// Update viewers
	std::vector<size_t> unpaired_viewer_indexes;
	{
		// Our node doesn't have bounds yet, so for now viewers are always paired.
		// TODO Update: the node has bounds now, need to change this

		// Destroyed viewers
		for (size_t i = 0; i < _paired_viewers.size(); ++i) {
			PairedViewer &p = _paired_viewers[i];
			if (!VoxelServer::get_singleton()->viewer_exists(p.id)) {
				PRINT_VERBOSE("Detected destroyed viewer in VoxelTerrain");
				p.state.view_distance_blocks = 0;
				unpaired_viewer_indexes.push_back(i);
			}
		}

		const Transform local_to_world_transform = get_global_transform();
		const Transform world_to_local_transform = local_to_world_transform.affine_inverse();

		// Note, this does not support non-uniform scaling
		// TODO There is probably a better way to do this
		const float view_distance_scale = world_to_local_transform.basis.xform(Vector3(1, 0, 0)).length();

		// New viewers and updates
		VoxelServer::get_singleton()->for_each_viewer([this, view_distance_scale, world_to_local_transform](
															  const VoxelServer::Viewer &viewer, uint32_t viewer_id) {
			size_t i;
			if (!try_get_paired_viewer_index(viewer_id, i)) {
				PairedViewer p;
				p.id = viewer_id;
				i = _paired_viewers.size();
				_paired_viewers.push_back(p);
			}

			PairedViewer &p = _paired_viewers[i];

			p.prev_state = p.state;

			const unsigned int view_distance_voxels =
					static_cast<unsigned int>(static_cast<float>(viewer.view_distance) * view_distance_scale);
			const Vector3 local_position = world_to_local_transform.xform(viewer.world_position);

			p.state.view_distance_blocks =
					min(view_distance_voxels >> get_block_size_pow2(), _max_view_distance_blocks);
			p.state.block_position = _map.voxel_to_block(Vector3i(local_position));
			p.state.requires_collisions = VoxelServer::get_singleton()->is_viewer_requiring_collisions(viewer_id);
			p.state.requires_meshes = VoxelServer::get_singleton()->is_viewer_requiring_visuals(viewer_id);
		});
	}

	const bool stream_enabled = _stream.is_valid() &&
								(Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor);

	// Find out which blocks need to appear and which need to be unloaded
	if (stream_enabled) {
		VOXEL_PROFILE_SCOPE();

		const uint32_t block_size = 1 << get_block_size_pow2();
		const Rect3i bounds_in_blocks = _bounds_in_voxels.downscaled(block_size);
		const Rect3i prev_bounds_in_blocks = _prev_bounds_in_voxels.downscaled(block_size);

		for (size_t i = 0; i < _paired_viewers.size(); ++i) {
			const PairedViewer &viewer = _paired_viewers[i];

			const Rect3i new_box = Rect3i::from_center_extents(
					viewer.state.block_position, Vector3i(viewer.state.view_distance_blocks))
										   .clipped(bounds_in_blocks);
			const Rect3i prev_box = Rect3i::from_center_extents(
					viewer.prev_state.block_position, Vector3i(viewer.prev_state.view_distance_blocks))
											.clipped(prev_bounds_in_blocks);

			if (prev_box != new_box) {
				// Unview blocks that just fell out of range
				prev_box.difference(new_box, [this, &viewer](Rect3i out_of_range_box) {
					out_of_range_box.for_each_cell([this, &viewer](Vector3i bpos) {
						unview_block(bpos, true,
								viewer.prev_state.requires_meshes,
								viewer.prev_state.requires_collisions);
					});
				});

				// View blocks that just entered the range
				new_box.difference(prev_box, [this, &viewer](Rect3i box_to_load) {
					box_to_load.for_each_cell([this, &viewer](Vector3i bpos) {
						// Load or update block
						view_block(bpos, true,
								viewer.state.requires_meshes,
								viewer.state.requires_collisions);
					});
				});
			}

			// Blocks that remained within range of the viewer may need some changes too if viewer flags were modified.
			// This operates on a DISTINCT set of blocks than the one above.

			if (viewer.state.requires_collisions != viewer.prev_state.requires_collisions) {
				const Rect3i box = new_box.clipped(prev_box);
				if (viewer.state.requires_collisions) {
					box.for_each_cell([this](Vector3i bpos) {
						view_block(bpos, false, false, true);
					});

				} else {
					box.for_each_cell([this](Vector3i bpos) {
						unview_block(bpos, false, false, true);
					});
				}
			}

			if (viewer.state.requires_meshes != viewer.prev_state.requires_meshes) {
				const Rect3i box = new_box.clipped(prev_box);
				if (viewer.state.requires_meshes) {
					box.for_each_cell([this](Vector3i bpos) {
						view_block(bpos, false, true, false);
					});

				} else {
					box.for_each_cell([this](Vector3i bpos) {
						unview_block(bpos, false, true, false);
					});
				}
			}
		}

		// We're done remembering the difference
		_prev_bounds_in_voxels = _bounds_in_voxels;
	}

	_stats.time_detect_required_blocks = profiling_clock.restart();

	// We no longer need unpaired viewers
	for (size_t i = 0; i < unpaired_viewer_indexes.size(); ++i) {
		PRINT_VERBOSE("Unpairing viewer from VoxelTerrain");
		size_t vi = unpaired_viewer_indexes[i];
		_paired_viewers[vi] = _paired_viewers.back();
		_paired_viewers.pop_back();
	}

	// It's possible the user didn't set a stream yet, or it is turned off
	if (stream_enabled) {
		send_block_data_requests();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters. It should only happen on first load, though.
	{
		VOXEL_PROFILE_SCOPE();

		//print_line(String("Receiving {0} blocks").format(varray(output.emerged_blocks.size())));
		for (size_t i = 0; i < _reception_buffers.data_output.size(); ++i) {
			const VoxelServer::BlockDataOutput &ob = _reception_buffers.data_output[i];

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

			CRASH_COND(ob.voxels.is_null());

			const Vector3i expected_block_size = Vector3i(_map.get_block_size());
			if (ob.voxels->get_size() != expected_block_size) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT(String("Block size obtained from stream is different from expected size. "
								 "Expected {0}, got {1}")
								  .format(varray(expected_block_size.to_vec3(), ob.voxels->get_size().to_vec3())));
				++_stats.dropped_block_loads;
				continue;
			}

			// Create or update block data
			VoxelBlock *block = _map.get_block(block_pos);
			const bool was_not_loaded = block == nullptr;
			block = _map.set_block_buffer(block_pos, ob.voxels);
			block->set_world(get_world());

			if (was_not_loaded) {
				// Set viewers count that are currently expecting the block
				block->viewers = loading_block.viewers;
			}

			emit_block_loaded(block);

			// TODO The following code appears to have order-dependency with block loading.
			// i.e if block loading responses arrive in a different order they were requested in,
			// some blocks will be stuck in LOAD. For now I made it so no re-ordering happens,
			// but it needs to be made more robust

			// Trigger mesh updates
			if (was_not_loaded) {
				// All neighbors have to be checked. If they are now surrounded, they can be updated
				Vector3i ndir;
				for (ndir.z = -1; ndir.z < 2; ++ndir.z) {
					for (ndir.x = -1; ndir.x < 2; ++ndir.x) {
						for (ndir.y = -1; ndir.y < 2; ++ndir.y) {
							const Vector3i npos = block_pos + ndir;

							// TODO What if the map is really composed of empty blocks?
							if (_map.is_block_surrounded(npos)) {
								VoxelBlock *nblock = _map.get_block(npos);
								if (nblock == nullptr || nblock->get_mesh_state() == VoxelBlock::MESH_UPDATE_NOT_SENT) {
									// Assuming it is scheduled to be updated already.
									// In case of BLOCK_UPDATE_SENT, we'll have to resend it.
									continue;
								}

								nblock->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
								_blocks_pending_update.push_back(npos);
							}
						}
					}
				}

			} else {
				// Only update the block, neighbors will probably follow if needed
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
				_blocks_pending_update.push_back(block_pos);
				//OS::get_singleton()->print("Update (%i, %i, %i)\n", block_pos.x, block_pos.y, block_pos.z);
			}
		}

		_reception_buffers.data_output.clear();

		if (stream_enabled) {
			send_block_data_requests();
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh updates
	{
		VOXEL_PROFILE_SCOPE();

		for (size_t bi = 0; bi < _blocks_pending_update.size(); ++bi) {
			const Vector3i block_pos = _blocks_pending_update[bi];

			// Check if the block is worth meshing
			// Smooth meshing works on more neighbors, so checking a single block isn't enough to ignore it,
			// but that will slow down meshing a lot.
			// TODO This is one reason to separate terrain systems between blocky and smooth (other reason is LOD)
			if (!(_stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF))) {
				VoxelBlock *block = _map.get_block(block_pos);
				if (block == nullptr) {
					continue;
				} else {
					CRASH_COND(block->voxels.is_null());

					bool is_empty;
					{
						RWLockRead lock(block->voxels->get_lock());
						is_empty = block->voxels->is_uniform(VoxelBuffer::CHANNEL_TYPE) &&
								   block->voxels->is_uniform(VoxelBuffer::CHANNEL_SDF) &&
								   block->voxels->get_voxel(0, 0, 0, VoxelBuffer::CHANNEL_TYPE) == Voxel::AIR_ID;
					}

					if (is_empty) {
						// If we got here, it must have been because of scheduling an update
						CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT);

						// The block contains empty voxels

						block->drop_mesh();
						block->drop_collision();
						block->set_mesh_state(VoxelBlock::MESH_UP_TO_DATE);

						// Optional, but I guess it might spare some memory.
						// Not doing it anymore cuz now we need to be more careful about multithreaded access.
						//block->voxels->clear_channel(VoxelBuffer::CHANNEL_TYPE, air_type);

						continue;
					}
				}
			}

			VoxelBlock *block = _map.get_block(block_pos);

			// If we got here, it must have been because of scheduling an update
			CRASH_COND(block == nullptr);
			CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT);

			// Get block and its neighbors
			VoxelServer::BlockMeshInput mesh_request;
			mesh_request.position = block_pos;
			mesh_request.lod = 0;
			for (unsigned int i = 0; i < Cube::MOORE_AREA_3D_COUNT; ++i) {
				const Vector3i npos = block_pos + Cube::g_ordered_moore_area_3d[i];
				VoxelBlock *nblock = _map.get_block(npos);
				// The block can actually be null on some occasions. Not sure yet if it's that bad
				//CRASH_COND(nblock == nullptr);
				if (nblock == nullptr) {
					continue;
				}
				mesh_request.blocks[i] = nblock->voxels;
			}

			VoxelServer::get_singleton()->request_block_mesh(_volume_id, mesh_request);

			block->set_mesh_state(VoxelBlock::MESH_UPDATE_SENT);
		}

		_blocks_pending_update.clear();
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// Receive mesh updates
	{
		VOXEL_PROFILE_SCOPE_NAMED("Receive mesh updates");

		OS &os = *OS::get_singleton();

		const uint32_t timeout = os.get_ticks_msec() + VoxelConstants::MAIN_THREAD_MESHING_BUDGET_MS;
		size_t queue_index = 0;

		const Transform local_to_world_transform = get_global_transform();

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		for (; queue_index < _reception_buffers.mesh_output.size() && os.get_ticks_msec() < timeout; ++queue_index) {
			const VoxelServer::BlockMeshOutput &ob = _reception_buffers.mesh_output[queue_index];

			VoxelBlock *block = _map.get_block(ob.position);
			if (block == nullptr) {
				// That block is no longer loaded, drop the result
				++_stats.dropped_block_meshs;
				continue;
			}

			if (ob.type == VoxelServer::BlockMeshOutput::TYPE_DROPPED) {
				// That block is loaded, but its meshing request was dropped.
				// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
				PRINT_VERBOSE("Received a block mesh drop while we were still expecting it");
				++_stats.dropped_block_meshs;
				continue;
			}

			Ref<ArrayMesh> mesh;
			mesh.instance();

			Vector<Array> collidable_surfaces; //need to put both blocky and smooth surfaces into one list

			VOXEL_PROFILE_SCOPE_NAMED("Build mesh");

			int surface_index = 0;
			for (int i = 0; i < ob.blocky_surfaces.surfaces.size(); ++i) {
				Array surface = ob.blocky_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				if (!is_surface_triangulated(surface)) {
					continue;
				}

				collidable_surfaces.push_back(surface);

				mesh->add_surface_from_arrays(
						ob.blocky_surfaces.primitive_type, surface, Array(), ob.blocky_surfaces.compression_flags);
				mesh->surface_set_material(surface_index, _materials[i]);
				++surface_index;
			}

			for (int i = 0; i < ob.smooth_surfaces.surfaces.size(); ++i) {
				Array surface = ob.smooth_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				if (!is_surface_triangulated(surface)) {
					continue;
				}

				collidable_surfaces.push_back(surface);

				mesh->add_surface_from_arrays(
						ob.smooth_surfaces.primitive_type, surface, Array(), ob.smooth_surfaces.compression_flags);
				mesh->surface_set_material(surface_index, _materials[i]);
				++surface_index;
			}

			if (is_mesh_empty(mesh)) {
				mesh = Ref<Mesh>();
				collidable_surfaces.clear();
			}

			const bool gen_collisions =
					_generate_collisions && block->viewers.get(VoxelViewerRefCount::TYPE_COLLISION) > 0;

			block->set_mesh(mesh);
			if (gen_collisions) {
				block->set_collision_mesh(collidable_surfaces, get_tree()->is_debugging_collisions_hint(), this);
			}
			block->set_parent_visible(is_visible());
			block->set_parent_transform(local_to_world_transform);
		}

		shift_up(_reception_buffers.mesh_output, queue_index);
	}

	_stats.time_process_update_responses = profiling_clock.restart();

	//print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") + String::num(_block_update_queue.size()));
}

Ref<VoxelTool> VoxelTerrain::get_voxel_tool() {
	Ref<VoxelTool> vt = memnew(VoxelToolTerrain(this));
	if (_stream.is_valid()) {
		if (_stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF)) {
			vt->set_channel(VoxelBuffer::CHANNEL_SDF);
		} else {
			vt->set_channel(VoxelBuffer::CHANNEL_TYPE);
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

void VoxelTerrain::set_bounds(Rect3i box) {
	_bounds_in_voxels = box.clipped(
			Rect3i::from_center_extents(Vector3i(), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT)));

	// Round to block size
	_bounds_in_voxels = _bounds_in_voxels.snapped(1 << get_block_size_pow2());

	const unsigned int largest_dimension = static_cast<unsigned int>(max(max(box.size.x, box.size.y), box.size.z));
	if (largest_dimension > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
		// Cap view distance to make sure you don't accidentally blow up memory when changing parameters
		if (_max_view_distance_blocks > MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME) {
			_max_view_distance_blocks = min(_max_view_distance_blocks, MAX_VIEW_DISTANCE_FOR_LARGE_VOLUME);
			_change_notify();
		}
	}
	// TODO Editor gizmo bounds
}

Rect3i VoxelTerrain::get_bounds() const {
	return _bounds_in_voxels;
}

Vector3 VoxelTerrain::_b_voxel_to_block(Vector3 pos) {
	return Vector3i(_map.voxel_to_block(pos)).to_vec3();
}

Vector3 VoxelTerrain::_b_block_to_voxel(Vector3 pos) {
	return Vector3i(_map.block_to_voxel(pos)).to_vec3();
}

void VoxelTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

// Explicitely ask to save a block if it was modified
void VoxelTerrain::_b_save_block(Vector3 p_block_pos) {
	const Vector3i block_pos(p_block_pos);

	VoxelBlock *block = _map.get_block(block_pos);
	ERR_FAIL_COND(block == nullptr);

	if (!block->is_modified()) {
		return;
	}

	ScheduleSaveAction{ _blocks_to_save, true }(block);
}

void VoxelTerrain::_b_set_bounds(AABB aabb) {
	// TODO Please Godot, have an integer AABB!
	set_bounds(Rect3i(aabb.position.round(), aabb.size.round()));
}

AABB VoxelTerrain::_b_get_bounds() const {
	const Rect3i b = get_bounds();
	return AABB(b.pos.to_vec3(), b.size.to_vec3());
}

void VoxelTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelTerrain::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelTerrain::get_stream);

	ClassDB::bind_method(D_METHOD("set_voxel_library", "library"), &VoxelTerrain::set_voxel_library);
	ClassDB::bind_method(D_METHOD("get_voxel_library"), &VoxelTerrain::get_voxel_library);

	ClassDB::bind_method(D_METHOD("set_material", "id", "material"), &VoxelTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material", "id"), &VoxelTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_max_view_distance", "distance_in_voxels"), &VoxelTerrain::set_max_view_distance);
	ClassDB::bind_method(D_METHOD("get_max_view_distance"), &VoxelTerrain::get_max_view_distance);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelTerrain::_b_voxel_to_block);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelTerrain::_b_block_to_voxel);

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelTerrain::get_statistics);
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelTerrain::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelTerrain::_b_save_modified_blocks);
	ClassDB::bind_method(D_METHOD("save_block", "position"), &VoxelTerrain::_b_save_block);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor", "enable"), &VoxelTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelTerrain::is_stream_running_in_editor);

	// TODO Rename `_voxel_bounds`
	ClassDB::bind_method(D_METHOD("set_bounds"), &VoxelTerrain::_b_set_bounds);
	ClassDB::bind_method(D_METHOD("get_bounds"), &VoxelTerrain::_b_get_bounds);

	//ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelTerrain::_on_stream_params_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"),
			"set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voxel_library", PROPERTY_HINT_RESOURCE_TYPE, "VoxelLibrary"),
			"set_voxel_library", "get_voxel_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_view_distance"), "set_max_view_distance", "get_max_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"),
			"set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"),
			"set_run_stream_in_editor", "is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "bounds"), "set_bounds", "get_bounds");

	// TODO Add back access to block, but with an API securing multithreaded access
	ADD_SIGNAL(MethodInfo(VoxelStringNames::get_singleton()->block_loaded,
			PropertyInfo(Variant::VECTOR3, "position")));
	ADD_SIGNAL(MethodInfo(VoxelStringNames::get_singleton()->block_unloaded,
			PropertyInfo(Variant::VECTOR3, "position")));
}
