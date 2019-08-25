#include "voxel_terrain.h"
#include "../streams/voxel_stream_file.h"
#include "../streams/voxel_stream_test.h"
#include "../util/profiling_clock.h"
#include "../util/utility.h"
#include "../util/voxel_raycast.h"
#include "voxel_block.h"
#include "voxel_map.h"

#include <core/core_string_names.h>
#include <core/engine.h>
#include <core/os/os.h>
#include <scene/3d/mesh_instance.h>

VoxelTerrain::VoxelTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	_map = Ref<VoxelMap>(memnew(VoxelMap));

	_view_distance_blocks = 8;
	_last_view_distance_blocks = 0;

	_stream_thread = NULL;
	_block_updater = NULL;

	_generate_collision = true;
	_run_in_editor = false;
	_smooth_meshing_enabled = false;
}

VoxelTerrain::~VoxelTerrain() {
	print_line("Destroying VoxelTerrain");

	if (_stream_thread) {
		// Schedule saving of all modified blocks,
		// without copy because we are destroying the map anyways
		save_all_modified_blocks(false);

		memdelete(_stream_thread);
	}

	if (_block_updater) {
		memdelete(_block_updater);
	}
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
		p_list->push_back(PropertyInfo(Variant::OBJECT, "material/" + itos(i), PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial,SpatialMaterial"));
	}
}

void VoxelTerrain::set_stream(Ref<VoxelStream> p_stream) {
	if (p_stream == _stream) {
		return;
	}

	if (_stream.is_valid()) {
		if (_stream->is_connected(CoreStringNames::get_singleton()->changed, this, "_on_stream_params_changed")) {
			_stream->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_stream_params_changed");
		}
	}

	_stream = p_stream;

	if (_stream.is_valid()) {
		_stream->connect(CoreStringNames::get_singleton()->changed, this, "_on_stream_params_changed");
	}

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

	bool updater_was_running = _block_updater != nullptr;

	stop_streamer();
	stop_updater();

	reset_map();
	_set_block_size_po2(p_block_size_po2);

	if (_stream.is_valid()) {
		start_streamer();
	}
	if (updater_was_running) {
		start_updater();
	}
}

void VoxelTerrain::_set_block_size_po2(int p_block_size_po2) {
	_map->create(p_block_size_po2, 0);
}

unsigned int VoxelTerrain::get_block_size_pow2() const {
	return _map->get_block_size_pow2();
}

void VoxelTerrain::_on_stream_params_changed() {
	stop_streamer();

	bool was_updater_running = _block_updater != nullptr;
	stop_updater();

	Ref<VoxelStreamFile> file_stream = _stream;
	if (file_stream.is_valid()) {

		int stream_block_size_po2 = file_stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);
	}

	if (_stream.is_valid()) {
		start_streamer();
	}
	if (was_updater_running) {
		start_updater();
	}

	// The whole map might change, so make all area dirty
	// TODO Actually, we should regenerate the whole map, not just update all its blocks
	make_all_view_dirty_deferred();
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

	bool updater_was_running = _block_updater != nullptr;

	stop_updater();

	if (updater_was_running) {
		start_updater();
	}

	// Voxel appearance might completely change
	make_all_view_dirty_deferred();
}

void VoxelTerrain::set_generate_collision(bool enabled) {
	_generate_collision = enabled;
}

bool VoxelTerrain::get_generate_collision() const {
	return _generate_collision;
}

int VoxelTerrain::get_view_distance() const {
	return _view_distance_blocks * _map->get_block_size();
}

void VoxelTerrain::set_view_distance(int distance_in_voxels) {
	ERR_FAIL_COND(distance_in_voxels < 0)
	int d = distance_in_voxels / _map->get_block_size();
	if (d != _view_distance_blocks) {
		print_line(String("View distance changed from ") + String::num(_view_distance_blocks) + String(" blocks to ") + String::num(d));
		_view_distance_blocks = d;
		// Blocks too far away will be removed in _process, same for blocks to load
	}
}

void VoxelTerrain::set_viewer_path(NodePath path) {
	_viewer_path = path;
}

NodePath VoxelTerrain::get_viewer_path() const {
	return _viewer_path;
}

Spatial *VoxelTerrain::get_viewer(NodePath path) const {
	if (path.is_empty()) {
		return NULL;
	}
	if (!is_inside_tree()) {
		return NULL;
	}
	Node *node = get_node(path);
	if (node == NULL) {
		return NULL;
	}
	return Object::cast_to<Spatial>(node);
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

bool VoxelTerrain::is_smooth_meshing_enabled() const {
	return _smooth_meshing_enabled;
}

void VoxelTerrain::set_smooth_meshing_enabled(bool enabled) {
	if (_smooth_meshing_enabled != enabled) {
		_smooth_meshing_enabled = enabled;
		stop_updater();
		start_updater();
		make_all_view_dirty_deferred();
	}
}

void VoxelTerrain::make_block_dirty(Vector3i bpos) {
	// TODO Immediate update viewer distance?

	VoxelTerrain::BlockDirtyState *state = _dirty_blocks.getptr(bpos);

	if (state == NULL) {
		// The block is not dirty, so it will either be loaded or updated

		VoxelBlock *block = _map->get_block(bpos);

		if (block != nullptr) {

			_blocks_pending_update.push_back(bpos);
			_dirty_blocks[bpos] = BLOCK_UPDATE_NOT_SENT;

			if (!block->modified) {
				print_line(String("Marking block {0} as modified").format(varray(bpos.to_vec3())));
				block->modified = true;
			}

		} else {

			_blocks_pending_load.push_back(bpos);
			_dirty_blocks[bpos] = BLOCK_LOAD;
		}

	} else if (*state == BLOCK_UPDATE_SENT) {
		// The updater is already processing the block,
		// but the block was modified again so we schedule another update
		*state = BLOCK_UPDATE_NOT_SENT;
		_blocks_pending_update.push_back(bpos);
	}

	//OS::get_singleton()->print("Dirty (%i, %i, %i)", bpos.x, bpos.y, bpos.z);

	// TODO What if a block is made dirty, goes through threaded update, then gets changed again before it gets updated?
	// this will make the second change ignored, which is not correct!
}

namespace {
struct ScheduleSaveAction {

	std::vector<VoxelDataLoader::InputBlock> &blocks_to_save;
	bool with_copy;

	void operator()(VoxelBlock *block) {
		if (block->modified) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelDataLoader::InputBlock b;
			b.data.voxels_to_save = with_copy ? block->voxels->duplicate() : block->voxels;
			b.position = block->position;
			b.can_be_discarded = false;
			blocks_to_save.push_back(b);
			block->modified = false;
		}
	}
};
} // namespace

void VoxelTerrain::immerge_block(Vector3i bpos) {

	ERR_FAIL_COND(_map.is_null());

	// Note: no need to copy the block because it gets removed from the map anyways
	_map->remove_block(bpos, ScheduleSaveAction{ _blocks_to_save, false });

	_dirty_blocks.erase(bpos);
	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block
}

void VoxelTerrain::save_all_modified_blocks(bool with_copy) {

	ERR_FAIL_COND(_stream_thread == nullptr);

	// That may cause a stutter, so should be used when the player won't notice
	_map->for_all_blocks(ScheduleSaveAction{ _blocks_to_save, with_copy });

	// And flush immediately
	send_block_data_requests();
}

Dictionary VoxelTerrain::get_statistics() const {

	Dictionary stream = VoxelDataLoader::Mgr::to_dictionary(_stats.stream);
	stream["dropped_blocks"] = _stats.dropped_stream_blocks;

	Dictionary updater = VoxelMeshUpdater::Mgr::to_dictionary(_stats.updater);
	updater["updated_blocks"] = _stats.updated_blocks;
	updater["mesh_alloc_time"] = _stats.mesh_alloc_time;
	updater["dropped_blocks"] = _stats.dropped_updater_blocks;
	updater["remaining_main_thread_blocks"] = _stats.remaining_main_thread_blocks;

	Dictionary d;
	d["stream"] = stream;
	d["updater"] = updater;

	// Breakdown of time spent in _process
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_send_load_requests"] = _stats.time_send_load_requests;
	d["time_process_load_responses"] = _stats.time_process_load_responses;
	d["time_send_update_requests"] = _stats.time_send_update_requests;
	d["time_process_update_responses"] = _stats.time_process_update_responses;

	return d;
}

bool VoxelTerrain::is_block_dirty(Vector3i bpos) const {
	return _dirty_blocks.has(bpos);
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

void VoxelTerrain::make_all_view_dirty_deferred() {
	// This trick will regenerate all chunks in view, according to the view distance found during block updates.
	// The point of doing this instead of immediately scheduling updates is that it will
	// always use an up-to-date view distance, which is not necessarily loaded yet on initialization.
	_last_view_distance_blocks = 0;

	//	Vector3i radius(_view_distance_blocks, _view_distance_blocks, _view_distance_blocks);
	//	make_blocks_dirty(-radius, 2*radius);
}

void VoxelTerrain::start_updater() {

	ERR_FAIL_COND(_block_updater != nullptr);

	// TODO Thread-safe way to change those parameters
	VoxelMeshUpdater::MeshingParams params;
	params.smooth_surface = _smooth_meshing_enabled;
	params.library = _library;

	_block_updater = memnew(VoxelMeshUpdater(1, params));
}

void VoxelTerrain::stop_updater() {

	struct ResetMeshStateAction {
		void operator()(VoxelBlock *block) {
			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
			}
		}
	};

	if (_block_updater) {
		memdelete(_block_updater);
		_block_updater = NULL;
	}

	_blocks_pending_main_thread_update.clear();
	_blocks_pending_update.clear();

	ResetMeshStateAction a;
	_map->for_all_blocks(a);

	// TODO This will be redundant with the thing above
	const Vector3i *key = NULL;
	while ((key = _dirty_blocks.next(key))) {
		BlockDirtyState *s = _dirty_blocks.getptr(*key);
		if (*s == BLOCK_UPDATE_SENT) {
			*s = BLOCK_UPDATE_NOT_SENT;
		}
	}
}

void VoxelTerrain::start_streamer() {

	ERR_FAIL_COND(_stream_thread != nullptr);
	ERR_FAIL_COND(_stream.is_null());

	_stream_thread = memnew(VoxelDataLoader(1, _stream, get_block_size_pow2()));
}

void VoxelTerrain::stop_streamer() {

	if (_stream_thread) {
		memdelete(_stream_thread);
		_stream_thread = nullptr;
	}

	_blocks_pending_load.clear();
}

void VoxelTerrain::reset_map() {
	_map->create(get_block_size_pow2(), 0);
}

inline int get_border_index(int x, int max) {
	return x == 0 ? 0 : x != max ? 1 : 2;
}

void VoxelTerrain::make_voxel_dirty(Vector3i pos) {

	// Update the block in which the voxel is
	Vector3i bpos = _map->voxel_to_block(pos);
	make_block_dirty(bpos);
	//OS::get_singleton()->print("Dirty (%i, %i, %i)\n", bpos.x, bpos.y, bpos.z);

	// Update neighbor blocks if the voxel is touching a boundary

	Vector3i rpos = _map->to_local(pos);

	// TODO Thread-safe way of getting this parameter
	bool check_corners = true; //_mesher->get_occlusion_enabled();

	const int max = _map->get_block_size() - 1;

	if (rpos.x == 0)
		make_block_dirty(bpos - Vector3i(1, 0, 0));
	else if (rpos.x == max)
		make_block_dirty(bpos + Vector3i(1, 0, 0));

	if (rpos.y == 0)
		make_block_dirty(bpos - Vector3i(0, 1, 0));
	else if (rpos.y == max)
		make_block_dirty(bpos + Vector3i(0, 1, 0));

	if (rpos.z == 0)
		make_block_dirty(bpos - Vector3i(0, 0, 1));
	else if (rpos.z == max)
		make_block_dirty(bpos + Vector3i(0, 0, 1));

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

		int m = get_border_index(rpos.x, max) + 3 * get_border_index(rpos.z, max) + 9 * get_border_index(rpos.y, max);

		const int *ce_indexes = ce_indexes_lut[m];
		int ce_count = ce_counts[m];
		//OS::get_singleton()->print("m=%i, rpos=(%i, %i, %i)\n", m, rpos.x, rpos.y, rpos.z);

		for (int i = 0; i < ce_count; ++i) {
			// TODO Because it's about ambient occlusion across 1 voxel only,
			// we could optimize it even more by looking at neighbor voxels,
			// and discard the update if we know it won't change anything
			const int *normal = normals[ce_indexes[i]];
			Vector3i nbpos(bpos.x + normal[0], bpos.y + normal[1], bpos.z + normal[2]);
			//OS::get_singleton()->print("Corner dirty (%i, %i, %i)\n", nbpos.x, nbpos.y, nbpos.z);
			make_block_dirty(nbpos);
		}
	}
}

void VoxelTerrain::make_area_dirty(Rect3i box) {

	Vector3i min_pos = box.pos;
	Vector3i max_pos = box.pos + box.size - Vector3(1, 1, 1);

	// TODO Thread-safe way of getting this parameter
	bool check_corners = true; //_mesher->get_occlusion_enabled();
	if (check_corners) {

		min_pos -= Vector3i(1, 1, 1);
		max_pos += Vector3i(1, 1, 1);

	} else {

		Vector3i min_rpos = _map->to_local(min_pos);
		if (min_rpos.x == 0)
			--min_pos.x;
		if (min_rpos.y == 0)
			--min_pos.y;
		if (min_rpos.z == 0)
			--min_pos.z;

		const int max = _map->get_block_size() - 1;
		Vector3i max_rpos = _map->to_local(max_pos);
		if (max_rpos.x == max)
			++max_pos.x;
		if (max_rpos.y == max)
			++max_pos.y;
		if (max_rpos.z == max)
			++max_pos.z;
	}

	Vector3i min_block_pos = _map->voxel_to_block(min_pos);
	Vector3i max_block_pos = _map->voxel_to_block(max_pos);

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

	struct SetVisibilityAction {
		bool visible;
		SetVisibilityAction(bool v) :
				visible(v) {}
		void operator()(VoxelBlock *block) {
			block->set_visible(visible);
		}
	};

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE:
			if (_block_updater == nullptr) {
				start_updater();
			}
			set_process(true);
			break;

		case NOTIFICATION_PROCESS:
			if (!Engine::get_singleton()->is_editor_hint() || _run_in_editor) {
				_process();
			}
			break;

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(SetWorldAction(*get_world()));
		} break;

		case NOTIFICATION_EXIT_WORLD:
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(SetWorldAction(nullptr));
			break;

		case NOTIFICATION_VISIBILITY_CHANGED:
			ERR_FAIL_COND(_map.is_null());
			_map->for_all_blocks(SetVisibilityAction(is_visible()));
			break;

			// TODO Listen for transform changes

		default:
			break;
	}
}

static void remove_positions_outside_box(
		Vector<Vector3i> &positions,
		Rect3i box,
		HashMap<Vector3i, VoxelTerrain::BlockDirtyState, Vector3iHasher> &state_map) {

	for (int i = 0; i < positions.size(); ++i) {
		const Vector3i bpos = positions[i];
		if (!box.contains(bpos)) {
			int last = positions.size() - 1;
			positions.write[i] = positions[last];
			positions.resize(last);
			state_map.erase(bpos);
			--i;
		}
	}
}

void VoxelTerrain::get_viewer_block_pos_and_direction(Vector3i &out_block_pos, Vector3 &out_direction) {

	if (Engine::get_singleton()->is_editor_hint()) {

		// TODO Use editor's camera here
		out_block_pos = Vector3i();
		out_direction = Vector3(0, -1, 0);

	} else {
		// TODO Use viewport camera, much easier
		Spatial *viewer = get_viewer(_viewer_path);
		if (viewer) {

			out_block_pos = _map->voxel_to_block(viewer->get_translation());
			out_direction = -viewer->get_global_transform().basis.get_axis(Vector3::AXIS_Z);

		} else {

			out_block_pos = _last_viewer_block_pos;
			out_direction = Vector3(0, -1, 0);
		}
	}
}

void VoxelTerrain::send_block_data_requests() {

	VoxelDataLoader::Input input;

	get_viewer_block_pos_and_direction(input.priority_position, input.priority_direction);

	for (int i = 0; i < _blocks_pending_load.size(); ++i) {
		VoxelDataLoader::InputBlock input_block;
		input_block.position = _blocks_pending_load[i];
		input_block.lod = 0;
		input.blocks.push_back(input_block);
	}

	for (int i = 0; i < _blocks_to_save.size(); ++i) {
		print_line(String("Requesting save of block {0}").format(varray(_blocks_to_save[i].position.to_vec3())));
		input.blocks.push_back(_blocks_to_save[i]);
	}

	//print_line(String("Sending {0} block requests").format(varray(input.blocks_to_emerge.size())));
	_blocks_pending_load.clear();
	_blocks_to_save.clear();

	_stream_thread->push(input);
}

void VoxelTerrain::_process() {

	// TODO Should be able to run without library, tho!
	if (_library.is_null()) {
		return;
	}

	OS &os = *OS::get_singleton();

	ERR_FAIL_COND(_map.is_null());

	ProfilingClock profiling_clock;

	// Get viewer location
	// TODO Transform to local (Spatial Transform)
	Vector3i viewer_block_pos;
	Vector3 viewer_direction;
	get_viewer_block_pos_and_direction(viewer_block_pos, viewer_direction);

	// Find out which blocks need to appear and which need to be unloaded
	{
		//Vector3i viewer_block_pos_delta = _last_viewer_block_pos - viewer_block_pos;
		Rect3i new_box = Rect3i::from_center_extents(viewer_block_pos, Vector3i(_view_distance_blocks));
		Rect3i prev_box = Rect3i::from_center_extents(_last_viewer_block_pos, Vector3i(_last_view_distance_blocks));

		if (prev_box != new_box) {
			//print_line(String("Loaded area changed: from ") + prev_box.to_string() + String(" to ") + new_box.to_string());

			Rect3i bounds = Rect3i::get_bounding_box(prev_box, new_box);
			Vector3i max = bounds.pos + bounds.size;

			// TODO There should be a way to only iterate relevant blocks
			Vector3i pos;
			for (pos.z = bounds.pos.z; pos.z < max.z; ++pos.z) {
				for (pos.y = bounds.pos.y; pos.y < max.y; ++pos.y) {
					for (pos.x = bounds.pos.x; pos.x < max.x; ++pos.x) {

						bool prev_contains = prev_box.contains(pos);
						bool new_contains = new_box.contains(pos);

						if (prev_contains && !new_contains) {
							// Unload block
							immerge_block(pos);

						} else if (!prev_contains && new_contains) {
							// Load or update block
							make_block_dirty(pos);
						}
					}
				}
			}
		}

		// Eliminate pending blocks that aren't needed
		remove_positions_outside_box(_blocks_pending_load, new_box, _dirty_blocks);
		remove_positions_outside_box(_blocks_pending_update, new_box, _dirty_blocks);
	}

	_stats.time_detect_required_blocks = profiling_clock.restart();

	_last_view_distance_blocks = _view_distance_blocks;
	_last_viewer_block_pos = viewer_block_pos;

	send_block_data_requests();

	_stats.time_send_load_requests = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters. It should only happen on first load, though.
	{
		const unsigned int bs = _map->get_block_size();
		const Vector3i block_size(bs, bs, bs);

		VoxelDataLoader::Output output;
		_stream_thread->pop(output);
		//print_line(String("Receiving {0} blocks").format(varray(output.emerged_blocks.size())));

		_stats.stream = output.stats;
		_stats.dropped_stream_blocks = 0;

		for (int i = 0; i < output.blocks.size(); ++i) {

			const VoxelDataLoader::OutputBlock &ob = output.blocks[i];

			if (ob.data.type != VoxelDataLoader::TYPE_LOAD) {
				continue;
			}

			Vector3i block_pos = ob.position;

			{
				VoxelTerrain::BlockDirtyState *state = _dirty_blocks.getptr(block_pos);
				if (state == NULL || *state != BLOCK_LOAD) {
					// That block was not requested, drop it
					++_stats.dropped_stream_blocks;
					continue;
				}
			}

			if (ob.drop_hint) {
				// That block was dropped by the data loader thread, but we were still expecting it...
				// This is not good, because it means the loader is out of sync due to a bug.
				// TODO Implement recovery like `VoxelLodTerrain`?
				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1}, {2}, {3})")
								   .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));
				++_stats.dropped_stream_blocks;
				continue;
			}

			// Check return
			// TODO Shouldn't halt execution though, as it can bring the map in an invalid state!
			ERR_FAIL_COND(ob.data.voxels_loaded->get_size() != block_size);

			// TODO Discard blocks out of range

			// Store buffer
			bool update_neighbors = !_map->has_block(block_pos);
			_map->set_block_buffer(block_pos, ob.data.voxels_loaded);

			// TODO The following code appears to have order-dependency with block loading.
			// i.e if block loading responses arrive in a different order they were requested in,
			// some blocks will be stuck in LOAD. For now I made it so no re-ordering happens,
			// but it needs to be made more robust

			// Trigger mesh updates
			if (update_neighbors) {
				// All neighbors have to be checked. If they are now surrounded, they can be updated
				Vector3i ndir;
				for (ndir.z = -1; ndir.z < 2; ++ndir.z) {
					for (ndir.x = -1; ndir.x < 2; ++ndir.x) {
						for (ndir.y = -1; ndir.y < 2; ++ndir.y) {
							Vector3i npos = block_pos + ndir;
							// TODO What if the map is really composed of empty blocks?
							if (_map->is_block_surrounded(npos)) {

								VoxelTerrain::BlockDirtyState *state = _dirty_blocks.getptr(npos);
								if (state && *state == BLOCK_UPDATE_NOT_SENT) {
									// Assuming it is scheduled to be updated already.
									// In case of BLOCK_UPDATE_SENT, we'll have to resend it.
									continue;
								}

								_dirty_blocks[npos] = BLOCK_UPDATE_NOT_SENT;
								_blocks_pending_update.push_back(npos);
							}
						}
					}
				}

			} else {
				// Only update the block, neighbors will probably follow if needed
				_dirty_blocks[block_pos] = BLOCK_UPDATE_NOT_SENT;
				_blocks_pending_update.push_back(block_pos);
				//OS::get_singleton()->print("Update (%i, %i, %i)\n", block_pos.x, block_pos.y, block_pos.z);
			}
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh updates
	{
		VoxelMeshUpdater::Input input;
		input.priority_position = viewer_block_pos;
		input.priority_direction = viewer_direction;

		for (int i = 0; i < _blocks_pending_update.size(); ++i) {
			Vector3i block_pos = _blocks_pending_update[i];

			// Check if the block is worth meshing
			// Smooth meshing works on more neighbors, so checking a single block isn't enough to ignore it,
			// but that will slow down meshing a lot.
			// TODO This is one reason to separate terrain systems between blocky and smooth (other reason is LOD)
			if (!_smooth_meshing_enabled) {
				VoxelBlock *block = _map->get_block(block_pos);
				if (block == NULL) {
					continue;
				} else {
					CRASH_COND(block->voxels.is_null());

					int air_type = 0;
					if (
							block->voxels->is_uniform(Voxel::CHANNEL_TYPE) &&
							block->voxels->is_uniform(Voxel::CHANNEL_ISOLEVEL) &&
							block->voxels->get_voxel(0, 0, 0, Voxel::CHANNEL_TYPE) == air_type) {

						VoxelTerrain::BlockDirtyState *block_state = _dirty_blocks.getptr(block_pos);
						CRASH_COND(block_state == NULL);
						CRASH_COND(*block_state != BLOCK_UPDATE_NOT_SENT);

						// The block contains empty voxels
						block->set_mesh(Ref<Mesh>(), Ref<World>());
						_dirty_blocks.erase(block_pos);

						// Optional, but I guess it might spare some memory
						block->voxels->clear_channel(Voxel::CHANNEL_TYPE, air_type);

						continue;
					}
				}
			}

			VoxelTerrain::BlockDirtyState *block_state = _dirty_blocks.getptr(block_pos);
			CRASH_COND(block_state == NULL);
			CRASH_COND(*block_state != BLOCK_UPDATE_NOT_SENT);

			// Create buffer padded with neighbor voxels
			Ref<VoxelBuffer> nbuffer;
			nbuffer.instance();

			// TODO Make the buffer re-usable
			unsigned int block_size = _map->get_block_size();
			unsigned int padding = _block_updater->get_required_padding();
			nbuffer->create(block_size + 2 * padding, block_size + 2 * padding, block_size + 2 * padding);

			unsigned int channels_mask = (1 << VoxelBuffer::CHANNEL_TYPE) | (1 << VoxelBuffer::CHANNEL_ISOLEVEL);
			_map->get_buffer_copy(_map->block_to_voxel(block_pos) - Vector3i(padding), **nbuffer, channels_mask);

			VoxelMeshUpdater::InputBlock iblock;
			iblock.data.voxels = nbuffer;
			iblock.position = block_pos;
			input.blocks.push_back(iblock);

			*block_state = BLOCK_UPDATE_SENT;
		}

		_block_updater->push(input);
		_blocks_pending_update.clear();
	}

	_stats.time_send_update_requests = profiling_clock.restart();

	// Get mesh updates
	{
		{
			VoxelMeshUpdater::Output output;
			_block_updater->pop(output);

			_stats.updater = output.stats;
			_stats.updated_blocks = output.blocks.size();
			_stats.dropped_updater_blocks = 0;

			_blocks_pending_main_thread_update.append_array(output.blocks);
		}

		Ref<World> world = get_world();
		ProfilingClock profiling_mesh_clock;
		uint32_t timeout = os.get_ticks_msec() + 10;
		int queue_index = 0;

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		for (; queue_index < _blocks_pending_main_thread_update.size() && os.get_ticks_msec() < timeout; ++queue_index) {

			const VoxelMeshUpdater::OutputBlock &ob = _blocks_pending_main_thread_update[queue_index];

			VoxelTerrain::BlockDirtyState *state = _dirty_blocks.getptr(ob.position);
			if (state && *state == BLOCK_UPDATE_SENT) {
				_dirty_blocks.erase(ob.position);
			}

			VoxelBlock *block = _map->get_block(ob.position);
			if (block == NULL) {
				// That block is no longer loaded, drop the result
				++_stats.dropped_updater_blocks;
				continue;
			}

			if (ob.drop_hint) {
				// That block is loaded, but its meshing request was dropped.
				// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
				print_line("Received a block mesh drop while we were still expecting it");
				++_stats.dropped_updater_blocks;
				continue;
			}

			Ref<ArrayMesh> mesh;
			mesh.instance();

			int surface_index = 0;
			const VoxelMeshUpdater::OutputBlockData &data = ob.data;
			for (int i = 0; i < data.blocky_surfaces.surfaces.size(); ++i) {

				Array surface = data.blocky_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				mesh->add_surface_from_arrays(data.blocky_surfaces.primitive_type, surface);
				mesh->surface_set_material(surface_index, _materials[i]);
				++surface_index;
			}

			for (int i = 0; i < data.smooth_surfaces.surfaces.size(); ++i) {

				Array surface = data.smooth_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				mesh->add_surface_from_arrays(data.smooth_surfaces.primitive_type, surface);
				mesh->surface_set_material(surface_index, _materials[i]);
				++surface_index;
			}

			if (is_mesh_empty(mesh)) {
				mesh = Ref<Mesh>();
			}

			block->set_mesh(mesh, world);

			if (_generate_collision) {
				block->generate_collision(this);
			}
		}

		shift_up(_blocks_pending_main_thread_update, queue_index);

		_stats.mesh_alloc_time = profiling_mesh_clock.restart();
	}

	_stats.time_process_update_responses = profiling_clock.restart();

	//print_line(String("d:") + String::num(_dirty_blocks.size()) + String(", q:") + String::num(_block_update_queue.size()));
}

//void VoxelTerrain::block_removed(VoxelBlock & block) {
//    MeshInstance * mesh_instance = block.get_mesh_instance(*this);
//    if (mesh_instance) {
//        mesh_instance->queue_delete();
//    }
//}

struct _VoxelTerrainRaycastContext {
	VoxelTerrain &terrain;
	//unsigned int channel_mask;
};

static bool _raycast_binding_predicate(Vector3i pos, void *context_ptr) {

	ERR_FAIL_COND_V(context_ptr == NULL, false);
	_VoxelTerrainRaycastContext *context = (_VoxelTerrainRaycastContext *)context_ptr;
	VoxelTerrain &terrain = context->terrain;

	//unsigned int channel = context->channel;

	Ref<VoxelMap> map = terrain.get_map();
	int v0 = map->get_voxel(pos, Voxel::CHANNEL_TYPE);

	Ref<VoxelLibrary> lib_ref = terrain.get_voxel_library();
	if (lib_ref.is_null())
		return false;
	const VoxelLibrary &lib = **lib_ref;

	if (lib.has_voxel(v0) == false)
		return false;

	const Voxel &voxel = lib.get_voxel_const(v0);
	if (voxel.is_transparent() == false)
		return true;

	float v1 = map->get_voxel_f(pos.x, pos.y, pos.z, Voxel::CHANNEL_ISOLEVEL);
	return v1 < 0;
}

void VoxelTerrain::_make_area_dirty_binding(AABB aabb) {
	make_area_dirty(Rect3i(aabb.position, aabb.size));
}

Variant VoxelTerrain::_raycast_binding(Vector3 origin, Vector3 direction, real_t max_distance) {

	// TODO Transform input if the terrain is rotated (in the future it can be made a Spatial node)

	Vector3i hit_pos;
	Vector3i prev_pos;

	_VoxelTerrainRaycastContext context = { *this };

	if (voxel_raycast(origin, direction, _raycast_binding_predicate, &context, max_distance, hit_pos, prev_pos)) {

		Dictionary hit = Dictionary();
		hit["position"] = hit_pos.to_vec3();
		hit["prev_position"] = prev_pos.to_vec3();
		return hit;
	} else {
		return Variant(); // Null dictionary, no alloc
	}
}

Vector3 VoxelTerrain::_voxel_to_block_binding(Vector3 pos) {
	return Vector3i(_map->voxel_to_block(pos)).to_vec3();
}

Vector3 VoxelTerrain::_block_to_voxel_binding(Vector3 pos) {
	return Vector3i(_map->block_to_voxel(pos)).to_vec3();
}

// For debugging purpose
VoxelTerrain::BlockDirtyState VoxelTerrain::get_block_state(Vector3 p_bpos) const {
	Vector3i bpos = p_bpos;
	const VoxelTerrain::BlockDirtyState *state = _dirty_blocks.getptr(bpos);
	if (state) {
		return *state;
	} else {
		if (!_map->has_block(bpos))
			return BLOCK_NONE;
		return BLOCK_IDLE;
	}
}

void VoxelTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelTerrain::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelTerrain::get_stream);

	ClassDB::bind_method(D_METHOD("set_voxel_library", "library"), &VoxelTerrain::set_voxel_library);
	ClassDB::bind_method(D_METHOD("get_voxel_library"), &VoxelTerrain::get_voxel_library);

	ClassDB::bind_method(D_METHOD("set_material", "id", "material"), &VoxelTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material", "id"), &VoxelTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_generate_collision"), &VoxelTerrain::get_generate_collision);
	ClassDB::bind_method(D_METHOD("set_generate_collision", "enabled"), &VoxelTerrain::set_generate_collision);

	ClassDB::bind_method(D_METHOD("get_viewer_path"), &VoxelTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer_path", "path"), &VoxelTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("is_smooth_meshing_enabled"), &VoxelTerrain::is_smooth_meshing_enabled);
	ClassDB::bind_method(D_METHOD("set_smooth_meshing_enabled", "enabled"), &VoxelTerrain::set_smooth_meshing_enabled);

	ClassDB::bind_method(D_METHOD("get_storage"), &VoxelTerrain::get_map);

	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelTerrain::_voxel_to_block_binding);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelTerrain::_block_to_voxel_binding);

	ClassDB::bind_method(D_METHOD("make_voxel_dirty", "pos"), &VoxelTerrain::_make_voxel_dirty_binding);
	ClassDB::bind_method(D_METHOD("make_area_dirty", "aabb"), &VoxelTerrain::_make_area_dirty_binding);

	ClassDB::bind_method(D_METHOD("raycast", "origin", "direction", "max_distance"), &VoxelTerrain::_raycast_binding, DEFVAL(100));

	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelTerrain::get_statistics);
	ClassDB::bind_method(D_METHOD("get_block_state", "block_pos"), &VoxelTerrain::get_block_state);

	ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelTerrain::_on_stream_params_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voxel_library", PROPERTY_HINT_RESOURCE_TYPE, "VoxelLibrary"), "set_voxel_library", "get_voxel_library");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewer_path"), "set_viewer_path", "get_viewer_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collision"), "set_generate_collision", "get_generate_collision");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "smooth_meshing_enabled"), "set_smooth_meshing_enabled", "is_smooth_meshing_enabled");

	BIND_ENUM_CONSTANT(BLOCK_NONE);
	BIND_ENUM_CONSTANT(BLOCK_LOAD);
	BIND_ENUM_CONSTANT(BLOCK_UPDATE_NOT_SENT);
	BIND_ENUM_CONSTANT(BLOCK_UPDATE_SENT);
	BIND_ENUM_CONSTANT(BLOCK_IDLE);
}
