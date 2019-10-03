#include "voxel_lod_terrain.h"
#include "../math/rect3i.h"
#include "../streams/voxel_stream_file.h"
#include "../util/profiling_clock.h"
#include "../voxel_tool_lod_terrain.h"
#include "voxel_map.h"
#include <core/core_string_names.h>
#include <core/engine.h>

#ifdef VOXEL_DEBUG_BOXES
#include <scene/3d/mesh_instance.h>
#endif

const uint32_t MAIN_THREAD_MESHING_BUDGET_MS = 8;

VoxelLodTerrain::VoxelLodTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	print_line("Construct VoxelLodTerrain");

	_lods[0].map.instance();

	// TODO Being able to set a LOD smaller than the stream is probably a bad idea,
	// Because it prevents edits from propagating up to the last one, they will be left out of sync
	set_lod_count(4);
	set_lod_split_scale(3);
}

VoxelLodTerrain::~VoxelLodTerrain() {

	print_line("Destroy VoxelLodTerrain");

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

Ref<Material> VoxelLodTerrain::get_material() const {
	return _material;
}

void VoxelLodTerrain::set_material(Ref<Material> p_material) {
	_material = p_material;
}

Ref<VoxelStream> VoxelLodTerrain::get_stream() const {
	return _stream;
}

unsigned int VoxelLodTerrain::get_block_size() const {
	return _lods[0].map->get_block_size();
}

unsigned int VoxelLodTerrain::get_block_size_pow2() const {
	return _lods[0].map->get_block_size_pow2();
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {

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

void VoxelLodTerrain::_on_stream_params_changed() {

	stop_streamer();

	bool was_updater_running = _block_updater != nullptr;
	stop_updater();

	Ref<VoxelStreamFile> file_stream = _stream;
	if (file_stream.is_valid()) {

		int stream_block_size_po2 = file_stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);

		int stream_lod_count = file_stream->get_lod_count();
		_set_lod_count(min(stream_lod_count, get_lod_count()));
	}

	if (_stream.is_valid()) {
		start_streamer();
	}
	if (was_updater_running) {
		start_updater();
	}

	// The whole map might change, so make all area dirty
	// TODO Actually, we should regenerate the whole map, not just update all its blocks
	for (int i = 0; i < get_lod_count(); ++i) {
		Lod &lod = _lods[i];
		lod.last_view_distance_blocks = 0;
	}
}

void VoxelLodTerrain::set_block_size_po2(unsigned int p_block_size_po2) {

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

	_set_block_size_po2(p_block_size_po2);
	reset_maps();

	if (_stream.is_valid()) {
		start_streamer();
	}
	if (updater_was_running) {
		start_updater();
	}
}

void VoxelLodTerrain::_set_block_size_po2(int p_block_size_po2) {
	_lods[0].map->create(p_block_size_po2, 0);
}

// Marks intersecting blocks in the area as modified, updates LODs and schedules remeshing.
// The provided box must be at LOD0 coordinates.
void VoxelLodTerrain::post_edit_area(Rect3i p_box) {

	Rect3i box = p_box.padded(1);
	Rect3i bbox = box.downscaled(get_block_size());

	bbox.for_each_cell([this](Vector3i block_pos_lod0) {
		post_edit_block_lod0(block_pos_lod0);
	});
}

void VoxelLodTerrain::post_edit_block_lod0(Vector3i block_pos_lod0) {

	Lod &lod0 = _lods[0];
	VoxelBlock *block = lod0.map->get_block(block_pos_lod0);
	ERR_FAIL_COND(block == nullptr);

	block->set_modified(true);

	if (!block->get_needs_lodding()) {
		block->set_needs_lodding(true);
		lod0.blocks_pending_lodding.push_back(block_pos_lod0);
	}
}

Ref<VoxelTool> VoxelLodTerrain::get_voxel_tool() {
	VoxelToolLodTerrain *vt = memnew(VoxelToolLodTerrain(this, _lods[0].map));
	// Set to most commonly used channel on this kind of terrain
	vt->set_channel(VoxelBuffer::CHANNEL_ISOLEVEL);
	return Ref<VoxelTool>(vt);
}

int VoxelLodTerrain::get_view_distance() const {
	return _view_distance_voxels;
}

void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {

	ERR_FAIL_COND(p_distance_in_voxels <= 0);
	ERR_FAIL_COND(p_distance_in_voxels > 8192);

	// Note: this is a hint distance, the terrain will attempt to have this radius filled with loaded voxels.
	// It is possible for blocks to still load beyond that distance.
	_view_distance_voxels = p_distance_in_voxels;
}

Spatial *VoxelLodTerrain::get_viewer() const {
	if (!is_inside_tree()) {
		return nullptr;
	}
	if (_viewer_path.is_empty()) {
		return nullptr;
	}
	Node *node = get_node(_viewer_path);
	if (node == nullptr) {
		return nullptr;
	}
	return Object::cast_to<Spatial>(node);
}

void VoxelLodTerrain::start_updater() {

	ERR_FAIL_COND(_block_updater != nullptr);

	// TODO Thread-safe way to change those parameters
	VoxelMeshUpdater::MeshingParams params;
	params.smooth_surface = true;

	_block_updater = memnew(VoxelMeshUpdater(2, params));
}

void VoxelLodTerrain::stop_updater() {

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

	for (int i = 0; i < MAX_LOD; ++i) {

		Lod &lod = _lods[i];
		lod.blocks_pending_update.clear();

		if (lod.map.is_valid()) {
			ResetMeshStateAction a;
			lod.map->for_all_blocks(a);
		}
	}
}

void VoxelLodTerrain::start_streamer() {

	ERR_FAIL_COND(_stream_thread != nullptr);
	ERR_FAIL_COND(_stream.is_null());

	_stream_thread = memnew(VoxelDataLoader(1, _stream, get_block_size_pow2()));
}

void VoxelLodTerrain::stop_streamer() {

	if (_stream_thread) {
		memdelete(_stream_thread);
		_stream_thread = nullptr;
	}

	for (int i = 0; i < MAX_LOD; ++i) {
		Lod &lod = _lods[i];
		lod.blocks_to_load.clear();
	}
}

void VoxelLodTerrain::set_lod_split_scale(float p_lod_split_scale) {

	if (p_lod_split_scale == _lod_split_scale) {
		return;
	}

	_lod_split_scale = p_lod_split_scale;

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {

		OctreeItem &item = E->value();
		item.octree.set_split_scale(_lod_split_scale);

		// Because `set_split_scale` may clamp it...
		_lod_split_scale = item.octree.get_split_scale();
	}
}

float VoxelLodTerrain::get_lod_split_scale() const {
	return _lod_split_scale;
}

void VoxelLodTerrain::set_lod_count(int p_lod_count) {

	ERR_FAIL_COND(p_lod_count >= MAX_LOD);
	ERR_FAIL_COND(p_lod_count < 1);

	if (get_lod_count() != p_lod_count) {
		_set_lod_count(p_lod_count);
	}
}

void VoxelLodTerrain::_set_lod_count(int p_lod_count) {

	CRASH_COND(p_lod_count >= MAX_LOD);
	CRASH_COND(p_lod_count < 1);

	_lod_count = p_lod_count;

	LodOctree<bool>::NoDestroyAction nda;

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.create_from_lod_count(get_block_size(), p_lod_count, nda);
#ifdef VOXEL_DEBUG_BOXES
		destroy_octree_debug_box(item, E->key());
		create_octree_debug_box(item, E->key());
#endif
	}

	reset_maps();
}

void VoxelLodTerrain::reset_maps() {
	// Clears all blocks and reconfigures maps to account for new LOD count and block sizes

	for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {

		Lod &lod = _lods[lod_index];

		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < get_lod_count()) {

			if (lod.map.is_null()) {
				lod.map.instance();
			}
			lod.map->create(get_block_size_pow2(), lod_index);

		} else {

			if (lod.map.is_valid()) {
				lod.map.unref();
			}
		}
	}
}

int VoxelLodTerrain::get_lod_count() const {
	return _lod_count;
}

void VoxelLodTerrain::set_generate_collisions(bool enabled) {
	_generate_collisions = enabled;
}

void VoxelLodTerrain::set_collision_lod_count(int lod_count) {
	_collision_lod_count = CLAMP(lod_count, -1, get_lod_count());
}

int VoxelLodTerrain::get_collision_lod_count() const {
	return _collision_lod_count;
}

void VoxelLodTerrain::set_viewer_path(NodePath path) {
	_viewer_path = path;
}

NodePath VoxelLodTerrain::get_viewer_path() const {
	return _viewer_path;
}

int VoxelLodTerrain::get_block_region_extent() const {
	// This is the radius of blocks around the viewer in which we may load them.
	// It depends on the LOD split scale, which tells how close to a block we need to be for it to subdivide.
	// Each LOD is fractal so that value is the same for each of them.
	return static_cast<int>(_lod_split_scale) * 2 + 2;
}

Dictionary VoxelLodTerrain::get_block_info(Vector3 fbpos, int lod_index) const {
	// Gets some info useful for debugging
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const Lod &lod = _lods[lod_index];
	Vector3i bpos(fbpos);

	bool meshed = false;
	bool visible = false;
	int loading_state = 0;
	const VoxelBlock *block = lod.map->get_block(bpos);
	if (block) {
		meshed = !block->has_mesh() && block->get_mesh_state() != VoxelBlock::MESH_UP_TO_DATE;
		visible = block->is_visible();
		loading_state = 2;
	} else if (lod.loading_blocks.has(bpos)) {
		loading_state = 1;
	}

#ifdef TOOLS_ENABLED
	int debug_unexpected_drop_time = -10000;
	const int *ptr = lod.debug_unexpected_load_drop_time.getptr(bpos);
	if (ptr) {
		debug_unexpected_drop_time = *ptr;
	}
	d["debug_unexpected_drop_time"] = debug_unexpected_drop_time;
#endif

	d["loading"] = loading_state;
	d["meshed"] = meshed;
	d["visible"] = visible;
	return d;
}

Vector3 VoxelLodTerrain::voxel_to_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3());
	const Lod &lod = _lods[lod_index];
	Vector3i bpos = lod.map->voxel_to_block(Vector3i(vpos)) >> lod_index;
	return bpos.to_vec3();
}

void VoxelLodTerrain::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_ENTER_TREE:
			if (_block_updater == nullptr) {
				start_updater();
			}
			set_process(true);
			break;

		case NOTIFICATION_PROCESS:
			if (!Engine::get_singleton()->is_editor_hint()) {
				_process();
			}
			break;

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			World *world = *get_world();
			for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
				if (_lods[lod_index].map.is_valid()) {
					_lods[lod_index].map->for_all_blocks([world](VoxelBlock *block) {
						block->set_world(world);
					});
				}
			}
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
				if (_lods[lod_index].map.is_valid()) {
					_lods[lod_index].map->for_all_blocks([](VoxelBlock *block) {
						block->set_world(nullptr);
					});
				}
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			bool visible = is_visible();
			for (int lod_index = 0; lod_index < MAX_LOD; ++lod_index) {
				if (_lods[lod_index].map.is_valid()) {
					_lods[lod_index].map->for_all_blocks([visible](VoxelBlock *block) {
						block->set_parent_visible(visible);
					});
				}
			}
		} break;

			// TODO Listen for transform changes

		default:
			break;
	}
}

void VoxelLodTerrain::get_viewer_pos_and_direction(Vector3 &out_pos, Vector3 &out_direction) const {

	if (Engine::get_singleton()->is_editor_hint()) {

		// TODO Use editor's camera here
		out_pos = Vector3();
		out_direction = Vector3(0, -1, 0);

	} else {
		// TODO Have option to use viewport camera
		Spatial *viewer = get_viewer();
		if (viewer) {

			Transform gt = viewer->get_global_transform();
			out_pos = gt.origin;
			out_direction = -gt.basis.get_axis(Vector3::AXIS_Z);

		} else {

			// TODO Just remember last viewer pos
			out_pos = (_lods[0].last_viewer_block_pos << _lods[0].map->get_block_size_pow2()).to_vec3();
			out_direction = Vector3(0, -1, 0);
		}
	}
}

void VoxelLodTerrain::try_schedule_loading_with_neighbors(const Vector3i &p_bpos, int lod_index) {
	CRASH_COND(lod_index < 0);
	CRASH_COND(lod_index >= get_lod_count());
	Lod &lod = _lods[lod_index];

	Vector3i bpos;

	for (int y = -1; y < 2; ++y) {
		for (int z = -1; z < 2; ++z) {
			for (int x = -1; x < 2; ++x) {

				bpos.x = p_bpos.x + x;
				bpos.y = p_bpos.y + y;
				bpos.z = p_bpos.z + z;

				VoxelBlock *block = lod.map->get_block(bpos);

				if (block == nullptr) {
					if (!lod.loading_blocks.has(bpos)) {
						lod.blocks_to_load.push_back(bpos);
						lod.loading_blocks.insert(bpos);
					}
				}
			}
		}
	}
}

bool VoxelLodTerrain::check_block_loaded_and_updated(const Vector3i &p_bpos, int lod_index) {
	CRASH_COND(lod_index < 0);
	CRASH_COND(lod_index >= get_lod_count());

	Lod &lod = _lods[lod_index];
	VoxelBlock *block = lod.map->get_block(p_bpos);

	if (block == nullptr) {
		try_schedule_loading_with_neighbors(p_bpos, lod_index);
		return false;
	}

	return check_block_loaded_and_updated(block);
}

bool VoxelLodTerrain::check_block_loaded_and_updated(VoxelBlock *block) {
	CRASH_COND(block == nullptr);
	Lod &lod = _lods[block->lod_index];

	switch (block->get_mesh_state()) {

		case VoxelBlock::MESH_NEVER_UPDATED:
		case VoxelBlock::MESH_NEED_UPDATE:
			if (lod.map->is_block_surrounded(block->position)) {

				lod.blocks_pending_update.push_back(block->position);
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);

			} else {
				try_schedule_loading_with_neighbors(block->position, block->lod_index);
			}
			return false;

		case VoxelBlock::MESH_UPDATE_NOT_SENT:
		case VoxelBlock::MESH_UPDATE_SENT:
			return false;

		default: // MESH_UP_TO_DATE
			break;
	}

	return true;
}

template <typename T, typename F>
inline void unordered_remove_if(std::vector<T> &vec, F predicate) {
	for (unsigned int i = 0; i < vec.size(); ++i) {
		if (predicate(vec[i])) {
			vec[i] = vec.back();
			vec.pop_back();
			--i;
		}
	}
}

void VoxelLodTerrain::send_block_data_requests() {

	VoxelDataLoader::Input input;

	Vector3 viewer_pos;
	get_viewer_pos_and_direction(viewer_pos, input.priority_direction);
	input.priority_position = _lods[0].map->voxel_to_block(Vector3i(viewer_pos));

	input.use_exclusive_region = true;
	// The last LOD may spread until end of view distance, it should not be discarded
	input.exclusive_region_max_lod = get_lod_count() - 1;
	input.exclusive_region_extent = get_block_region_extent();

	for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		for (unsigned int i = 0; i < lod.blocks_to_load.size(); ++i) {
			VoxelDataLoader::InputBlock input_block;
			input_block.position = lod.blocks_to_load[i];
			input_block.lod = lod_index;
			input.blocks.push_back(input_block);
		}

		lod.blocks_to_load.clear();
	}

	for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
		print_line(String("Requesting save of block {0} lod {1}")
						   .format(varray(_blocks_to_save[i].position.to_vec3(), _blocks_to_save[i].lod)));
		input.blocks.push_back(_blocks_to_save[i]);
	}

	_blocks_to_save.clear();

	//print_line(String("Sending {0}").format(varray(input.blocks_to_emerge.size())));
	_stream_thread->push(input);
}

void VoxelLodTerrain::_process() {

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	OS &os = *OS::get_singleton();

	// Get viewer location
	// TODO Transform to local (Spatial Transform)
	Vector3 viewer_pos;
	Vector3 viewer_direction;
	get_viewer_pos_and_direction(viewer_pos, viewer_direction);
	Vector3i viewer_block_pos = _lods[0].map->voxel_to_block(Vector3i(viewer_pos));

	_stats.dropped_block_loads = 0;
	_stats.dropped_block_meshs = 0;
	_stats.blocked_lods = 0;

	// Here we go...

	// Update pending LOD data modifications due to edits.
	// These are deferred from edits so we can batch them.
	// It has to happen first because blocks can be unloaded afterwards.
	flush_pending_lod_edits();

	ProfilingClock profiling_clock;

	// Unload blocks falling out of block region extent
	// TODO Obsoleted by octree grid?
	{
		// TODO Could it actually be enough to have a rolling update on all blocks?

		// This should be the same distance relatively to each LOD
		int block_region_extent = get_block_region_extent();

		for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			unsigned int block_size_po2 = _lods[0].map->get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod = VoxelMap::voxel_to_block_b(viewer_pos, block_size_po2);

			Rect3i new_box = Rect3i::from_center_extents(viewer_block_pos_within_lod, Vector3i(block_region_extent));
			Rect3i prev_box = Rect3i::from_center_extents(lod.last_viewer_block_pos, Vector3i(lod.last_view_distance_blocks));

			// Eliminate pending blocks that aren't needed

			// This vector must be empty at this point.
			// Let's assert so it will pop on your face the day that assumption changes
			CRASH_COND(!lod.blocks_to_load.empty());

			if (prev_box != new_box) {
				prev_box.difference(new_box, [this, lod_index](Rect3i out_of_range_box) {
					out_of_range_box.for_each_cell([=](Vector3i pos) {
						immerge_block(pos, lod_index);
					});
				});
			}

			// Cancel block updates that are not within the padded region (since neighbors are always required to remesh)
			Rect3i padded_new_box = new_box.padded(-1);
			unordered_remove_if(lod.blocks_pending_update, [&lod, padded_new_box](Vector3i bpos) {
				if (padded_new_box.contains(bpos)) {
					return false;
				} else {
					VoxelBlock *block = lod.map->get_block(bpos);
					if (block != nullptr) {
						block->set_mesh_state(VoxelBlock::MESH_NEED_UPDATE);
					}
					return true;
				}
			});

			lod.last_viewer_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_blocks = block_region_extent;
		}
	}

	// Create and remove octrees in a grid around the viewer
	{
		// TODO Investigate if multi-octree can produce cracks in the terrain (so far I haven't noticed)

		const unsigned int octree_size_po2 = get_block_size_pow2() + get_lod_count() - 1;
		const unsigned int octree_size = 1 << octree_size_po2;
		const unsigned int octree_region_extent = 1 + _view_distance_voxels / (1 << octree_size_po2);

		Vector3i viewer_octree_pos = (Vector3i(viewer_pos) + Vector3i(octree_size / 2)) >> octree_size_po2;

		Rect3i new_box = Rect3i::from_center_extents(viewer_octree_pos, Vector3i(octree_region_extent));
		Rect3i prev_box = _last_octree_region_box;

		if (new_box != prev_box) {

			struct CleanOctreeAction {
				VoxelLodTerrain *self;
				Vector3i block_offset_lod0;

				void operator()(LodOctree<bool>::Node *node, Vector3i node_pos, unsigned int lod_index) {
					Lod &lod = self->_lods[lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

					VoxelBlock *block = lod.map->get_block(bpos);
					if (block) {
						block->set_visible(false);
					}
				}
			};

			struct ExitAction {
				VoxelLodTerrain *self;
				void operator()(const Vector3i &pos) {

					Map<Vector3i, OctreeItem>::Element *E = self->_lod_octrees.find(pos);
					if (E == nullptr) {
						return;
					}

					OctreeItem &item = E->value();
					Vector3i block_pos_maxlod = E->key();

#ifdef VOXEL_DEBUG_BOXES
					self->destroy_octree_debug_box(item, block_pos_maxlod);
#endif

					int last_lod_index = self->get_lod_count() - 1;

					// We just drop the octree and hide blocks it was considering as visible.
					// Normally such octrees shouldn't bee too deep as they will likely be at the edge
					// of the loaded area, unless the player teleported far away.
					CleanOctreeAction a;
					a.self = self;
					a.block_offset_lod0 = block_pos_maxlod << last_lod_index;
					item.octree.clear(a);

					self->_lod_octrees.erase(E);
				}
			};

			struct EnterAction {
				VoxelLodTerrain *self;
				int block_size;
				void operator()(const Vector3i &pos) {
					// That's a new cell we are entering, shouldn't be anything there
					CRASH_COND(self->_lod_octrees.has(pos));

					// Create new octree
					// TODO Use ObjectPool to store them, deletion won't be cheap
					Map<Vector3i, OctreeItem>::Element *E = self->_lod_octrees.insert(pos, OctreeItem());
					CRASH_COND(E == nullptr);
					OctreeItem &item = E->value();
					LodOctree<bool>::NoDestroyAction nda;
					item.octree.create_from_lod_count(block_size, self->get_lod_count(), nda);
					item.octree.set_split_scale(self->_lod_split_scale);

#ifdef VOXEL_DEBUG_BOXES
					self->create_octree_debug_box(item, pos);
#endif
				}
			};

			ExitAction exit_action;
			exit_action.self = this;

			EnterAction enter_action;
			enter_action.self = this;
			enter_action.block_size = get_block_size();

			prev_box.difference(new_box, [exit_action](Rect3i out_of_range_box) {
				out_of_range_box.for_each_cell(exit_action);
			});

			new_box.difference(prev_box, [enter_action](Rect3i box_to_load) {
				box_to_load.for_each_cell(enter_action);
			});
		}

		_last_octree_region_box = new_box;
	}

	// Find which blocks we need to load and see, within each octree
	{
		// TODO Maintain a vector to make iteration faster?
		for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {

			OctreeItem &item = E->value();
			Vector3i block_pos_maxlod = E->key();
			Vector3i block_offset_lod0 = block_pos_maxlod << (get_lod_count() - 1);

			struct SubdivideAction {
				VoxelLodTerrain *self;
				Vector3i block_offset_lod0;
				unsigned int blocked_count = 0;

				bool can_do_root(unsigned int lod_index) {
					Vector3i offset = block_offset_lod0 >> lod_index;
					return self->check_block_loaded_and_updated(offset, lod_index);
				}

				bool can_do_children(LodOctree<bool>::Node *node, Vector3i node_pos, unsigned int child_lod_index) {

					Vector3i offset = block_offset_lod0 >> child_lod_index;
					bool can = true;

					// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
					for (int i = 0; i < 8; ++i) {

						// Get block pos local-to-region
						Vector3i child_pos = LodOctree<bool>::get_child_position(node_pos, i);

						// Convert to local-to-terrain
						child_pos += offset;

						// We have to ping ALL children, because the reason we are here is we want them loaded
						can &= self->check_block_loaded_and_updated(child_pos, child_lod_index);
					}

					if (!can) {
						++blocked_count;
					}

					return can;
				}

				bool operator()(LodOctree<bool>::Node *node, Vector3i node_pos, unsigned int lod_index) {

					Lod &lod = self->_lods[lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

					VoxelBlock *block = lod.map->get_block(bpos);

					CRASH_COND(block == nullptr);
					// Never show a block that hasn't been meshed
					CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UP_TO_DATE);

					block->set_visible(true);
					return true;
				}
			};

			struct UnsubdivideAction {
				VoxelLodTerrain *self;
				Vector3i block_offset_lod0;
				unsigned int blocked_count = 0;

				bool can_do(LodOctree<bool>::Node *node, Vector3i node_pos, unsigned int parent_lod_index) {

					// Can only unsubdivide if the parent mesh is ready
					Vector3i bpos = node_pos + (block_offset_lod0 >> parent_lod_index);

					bool can = self->check_block_loaded_and_updated(bpos, parent_lod_index);

					if (!can) {
						++blocked_count;
					}

					return can;
				}

				void operator()(LodOctree<bool>::Node *node, Vector3i node_pos, unsigned int lod_index) {

					Lod &lod = self->_lods[lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

					VoxelBlock *block = lod.map->get_block(bpos);
					if (block) {
						block->set_visible(false);
					}
				}
			};

			SubdivideAction subdivide_action;
			subdivide_action.self = this;
			subdivide_action.block_offset_lod0 = block_offset_lod0;

			UnsubdivideAction unsubdivide_action;
			unsubdivide_action.self = this;
			unsubdivide_action.block_offset_lod0 = block_offset_lod0;

			Vector3 relative_viewer_pos = viewer_pos - get_block_size() * block_offset_lod0.to_vec3();
			item.octree.update(relative_viewer_pos, subdivide_action, unsubdivide_action);

			// Ideally, this stat should stabilize to zero.
			// If not, something in block management prevents LODs to properly show up and should be fixed.
			_stats.blocked_lods += subdivide_action.blocked_count + unsubdivide_action.blocked_count;
		}
	}

	_stats.time_detect_required_blocks = profiling_clock.restart();

	send_block_data_requests();

	_stats.time_request_blocks_to_load = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	{
		VoxelDataLoader::Output output;
		_stream_thread->pop(output);
		_stats.stream = output.stats;

		//print_line(String("Loaded {0} blocks").format(varray(output.emerged_blocks.size())));

		for (int i = 0; i < output.blocks.size(); ++i) {

			const VoxelDataLoader::OutputBlock &ob = output.blocks[i];

			if (ob.lod >= get_lod_count()) {
				// That block was requested at a time where LOD was higher... drop it
				++_stats.dropped_block_loads;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			{
				Set<Vector3i>::Element *E = lod.loading_blocks.find(ob.position);
				if (E == nullptr) {
					// That block was not requested, or is no longer needed. drop it...
					++_stats.dropped_block_loads;
					continue;
				}

				lod.loading_blocks.erase(E);
			}

			if (ob.drop_hint) {
				// That block was dropped by the data loader thread, but we were still expecting it...
				// This is not good, because it means the loader is out of sync due to a bug.
				// We can recover with the removal from `loading_blocks` so it will be re-queried again later...
				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1}, {2}, {3})")
								   .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));
#ifdef TOOLS_ENABLED
				lod.debug_unexpected_load_drop_time[ob.position] = OS::get_singleton()->get_ticks_msec();
#endif
				++_stats.dropped_block_loads;
				continue;
			}

			if (ob.data.voxels_loaded->get_size() != lod.map->get_block_size()) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT("Block size obtained from stream is different from expected size");
				++_stats.dropped_block_loads;
				continue;
			}

			// Store buffer
			VoxelBlock *block = lod.map->set_block_buffer(ob.position, ob.data.voxels_loaded);
			//print_line(String("Adding block {0} at lod {1}").format(varray(eo.block_position.to_vec3(), eo.lod)));
			// The block will be made visible and meshed only by LodOctree
			block->set_visible(false);
			block->set_parent_visible(is_visible());
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh updates
	{
		VoxelMeshUpdater::Input input;
		input.priority_position = viewer_block_pos;
		input.priority_direction = viewer_direction;
		input.use_exclusive_region = true;
		input.exclusive_region_max_lod = get_lod_count() - 1;
		input.exclusive_region_extent = get_block_region_extent();

		for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			Lod &lod = _lods[lod_index];

			for (unsigned int i = 0; i < lod.blocks_pending_update.size(); ++i) {
				Vector3i block_pos = lod.blocks_pending_update[i];

				VoxelBlock *block = lod.map->get_block(block_pos);
				CRASH_COND(block == nullptr);
				// All blocks we get here must be in the scheduled state
				CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT);

				// TODO Perhaps we could do a bit of early-rejection before spending time in buffer copy?

				// Create buffer padded with neighbor voxels
				Ref<VoxelBuffer> nbuffer;
				nbuffer.instance();

				// TODO Make the buffer re-usable
				unsigned int block_size = lod.map->get_block_size();
				unsigned int padding = _block_updater->get_required_padding();
				nbuffer->create(
						block_size + 2 * padding,
						block_size + 2 * padding,
						block_size + 2 * padding);

				unsigned int channels_mask = (1 << VoxelBuffer::CHANNEL_ISOLEVEL);
				lod.map->get_buffer_copy(lod.map->block_to_voxel(block_pos) - Vector3i(padding), **nbuffer, channels_mask);

				VoxelMeshUpdater::InputBlock iblock;
				iblock.data.voxels = nbuffer;
				iblock.position = block_pos;
				iblock.lod = lod_index;
				input.blocks.push_back(iblock);

				block->set_mesh_state(VoxelBlock::MESH_UPDATE_SENT);
			}

			lod.blocks_pending_update.clear();
		}

		//print_line(String("Sending {0} updates").format(varray(input.blocks.size())));
		_block_updater->push(input);
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// Receive mesh updates
	{
		{
			VoxelMeshUpdater::Output output;
			_block_updater->pop(output);

			_stats.updater = output.stats;
			_stats.updated_blocks = output.blocks.size();

			for (int i = 0; i < output.blocks.size(); ++i) {
				const VoxelMeshUpdater::OutputBlock &ob = output.blocks[i];

				if (ob.lod >= get_lod_count()) {
					// Sorry, LOD configuration changed, drop that mesh
					++_stats.dropped_block_meshs;
					continue;
				}

				_blocks_pending_main_thread_update.push_back(ob);
			}
		}

		uint32_t timeout = os.get_ticks_msec() + MAIN_THREAD_MESHING_BUDGET_MS; // Allocate milliseconds max to upload meshes
		unsigned int queue_index = 0;

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		for (; queue_index < _blocks_pending_main_thread_update.size() && os.get_ticks_msec() < timeout; ++queue_index) {

			const VoxelMeshUpdater::OutputBlock &ob = _blocks_pending_main_thread_update[queue_index];

			if (ob.lod >= get_lod_count()) {
				// Sorry, LOD configuration changed, drop that mesh
				++_stats.dropped_block_meshs;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			VoxelBlock *block = lod.map->get_block(ob.position);
			if (block == NULL) {
				// That block is no longer loaded, drop the result
				++_stats.dropped_block_meshs;
				continue;
			}

			if (ob.drop_hint) {
				// That block is loaded, but its meshing request was dropped.
				// TODO Not sure what to do in this case, the code sending update queries has to be tweaked
				print_line("Received a block mesh drop while we were still expecting it");
				++_stats.dropped_block_meshs;
				continue;
			}

			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UP_TO_DATE);
			}

			Ref<ArrayMesh> mesh;
			mesh.instance();

			// TODO Allow multiple collision surfaces
			Array collidable_surface;

			unsigned int surface_index = 0;
			const VoxelMeshUpdater::OutputBlockData &data = ob.data;
			for (int i = 0; i < data.smooth_surfaces.surfaces.size(); ++i) {

				Array surface = data.smooth_surfaces.surfaces[i];
				if (surface.empty()) {
					continue;
				}

				CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
				if (!is_surface_triangulated(surface)) {
					continue;
				}

				if (collidable_surface.empty()) {
					collidable_surface = surface;
				}

				mesh->add_surface_from_arrays(data.smooth_surfaces.primitive_type, surface);
				mesh->surface_set_material(surface_index, _material);
				// No multi-material supported yet
				++surface_index;
			}

			if (is_mesh_empty(mesh)) {
				mesh = Ref<Mesh>();
			}

			bool has_collision = _generate_collisions;
			if (has_collision && _collision_lod_count != -1) {
				has_collision = ob.lod < _collision_lod_count;
			}

			block->set_mesh(mesh, this, has_collision, collidable_surface, get_tree()->is_debugging_collisions_hint());
		}

		shift_up(_blocks_pending_main_thread_update, queue_index);
	}

	_stats.time_process_update_responses = profiling_clock.restart();
}

void VoxelLodTerrain::flush_pending_lod_edits() {
	// Propagates edits performed so far to other LODs.
	// These LODs must be currently in memory, otherwise terrain data will miss it.
	// This is currently ensured by the fact we load blocks in a "pyramidal" way,
	// i.e there is no way for a block to be loaded if its parent LOD isn't loaded already.
	// In the future we may implement storing of edits to be applied later if blocks can't be found.

	//ProfilingClock profiling_clock;

	struct L {
		static inline void schedule_update(VoxelBlock *block, std::vector<Vector3i> &blocks_pending_update) {
			if (block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT) {
				if (block->is_visible()) {
					// Schedule an update
					block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
					blocks_pending_update.push_back(block->position);
				} else {
					// Just mark it as needing update, so the visibility system will schedule its update when needed
					block->set_mesh_state(VoxelBlock::MESH_NEED_UPDATE);
				}
			}
		}
	};

	// Make sure LOD0 gets updates even if _lod_count is 1
	Lod &lod0 = _lods[0];
	for (unsigned int i = 0; i < lod0.blocks_pending_lodding.size(); ++i) {
		Vector3i bpos = lod0.blocks_pending_lodding[i];
		VoxelBlock *block = lod0.map->get_block(bpos);
		block->set_needs_lodding(false);
		L::schedule_update(block, lod0.blocks_pending_update);
	}

	int half_bs = get_block_size() >> 1;

	// Process downscales upwards in pairs of consecutive LODs.
	// This ensures we don't process multiple times the same blocks.
	// Only LOD0 is editable at the moment, so we'll downscale from there
	for (int dst_lod_index = 1; dst_lod_index < _lod_count; ++dst_lod_index) {
		Lod &src_lod = _lods[dst_lod_index - 1];
		Lod &dst_lod = _lods[dst_lod_index];

		for (unsigned int i = 0; i < src_lod.blocks_pending_lodding.size(); ++i) {
			Vector3i src_bpos = src_lod.blocks_pending_lodding[i];
			Vector3i dst_bpos = src_bpos >> 1;

			VoxelBlock *src_block = src_lod.map->get_block(src_bpos);
			VoxelBlock *dst_block = dst_lod.map->get_block(dst_bpos);

			// The block and its lower LODs are expected to be available.
			// Otherwise it means the function was called too late
			CRASH_COND(src_block == nullptr);
			CRASH_COND(dst_block == nullptr);
			CRASH_COND(src_block->voxels.is_null());
			CRASH_COND(dst_block->voxels.is_null());

			L::schedule_update(dst_block, dst_lod.blocks_pending_update);

			src_block->set_needs_lodding(false);

			dst_block->set_modified(true);

			if (dst_lod_index != _lod_count - 1 && !dst_block->get_needs_lodding()) {
				dst_block->set_needs_lodding(true);
				dst_lod.blocks_pending_lodding.push_back(dst_bpos);
			}

			Vector3i rel = src_bpos - (dst_bpos << 1);

			// Update lower LOD
			// This must always be done after an edit before it gets saved, otherwise LODs won't match and it will look ugly.
			// TODO Try to narrow to edited region instead of taking whole block
			src_block->voxels->downscale_to(**dst_block->voxels, Vector3i(), src_block->voxels->get_size(), rel * half_bs);
		}

		src_lod.blocks_pending_lodding.clear();
	}

	//	uint64_t time_spent = profiling_clock.restart();
	//	if (time_spent > 10) {
	//		print_line(String("Took {0} us to update lods").format(varray(time_spent)));
	//	}
}

namespace {
struct ScheduleSaveAction {

	std::vector<VoxelDataLoader::InputBlock> &blocks_to_save;
	bool with_copy;

	void operator()(VoxelBlock *block) {
		if (block->is_modified()) {
			print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelDataLoader::InputBlock b;
			b.data.voxels_to_save = with_copy ? block->voxels->duplicate() : block->voxels;
			b.position = block->position;
			b.can_be_discarded = false;
			b.lod = block->lod_index;
			blocks_to_save.push_back(b);
			block->set_modified(false);
		}
	}
};
} // namespace

void VoxelLodTerrain::immerge_block(Vector3i block_pos, int lod_index) {

	ERR_FAIL_COND(lod_index >= get_lod_count());
	ERR_FAIL_COND(_lods[lod_index].map.is_null());

	Lod &lod = _lods[lod_index];

	lod.map->remove_block(block_pos, ScheduleSaveAction{ _blocks_to_save, false });

	lod.loading_blocks.erase(block_pos);

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block

	// No need to remove things from blocks_pending_load,
	// This vector is filled and cleared immediately in the main process.
	// It is a member only to re-use its capacity memory over frames.
}

void VoxelLodTerrain::save_all_modified_blocks(bool with_copy) {

	ERR_FAIL_COND(_stream_thread == nullptr);

	flush_pending_lod_edits();

	for (int i = 0; i < _lod_count; ++i) {
		// That may cause a stutter, so should be used when the player won't notice
		_lods[i].map->for_all_blocks(ScheduleSaveAction{ _blocks_to_save, with_copy });
	}

	// And flush immediately
	send_block_data_requests();
}

Dictionary VoxelLodTerrain::get_statistics() const {

	Dictionary d;
	d["stream"] = VoxelDataLoader::Mgr::to_dictionary(_stats.stream);
	d["updater"] = VoxelMeshUpdater::Mgr::to_dictionary(_stats.updater);

	// Breakdown of time spent in _process
	d["time_detect_required_blocks"] = _stats.time_detect_required_blocks;
	d["time_request_blocks_to_load"] = _stats.time_request_blocks_to_load;
	d["time_process_load_responses"] = _stats.time_process_load_responses;
	d["time_request_blocks_to_update"] = _stats.time_request_blocks_to_update;
	d["time_process_update_responses"] = _stats.time_process_update_responses;

	d["remaining_main_thread_blocks"] = (int)_blocks_pending_main_thread_update.size();
	d["dropped_block_loads"] = _stats.dropped_block_loads;
	d["dropped_block_meshs"] = _stats.dropped_block_meshs;
	d["updated_blocks"] = _stats.updated_blocks;
	d["blocked_lods"] = _stats.blocked_lods;

	return d;
}

void VoxelLodTerrain::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &VoxelLodTerrain::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &VoxelLodTerrain::get_stream);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelLodTerrain::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelLodTerrain::get_material);

	ClassDB::bind_method(D_METHOD("set_view_distance", "distance_in_voxels"), &VoxelLodTerrain::set_view_distance);
	ClassDB::bind_method(D_METHOD("get_view_distance"), &VoxelLodTerrain::get_view_distance);

	ClassDB::bind_method(D_METHOD("get_generate_collisions"), &VoxelLodTerrain::get_generate_collisions);
	ClassDB::bind_method(D_METHOD("set_generate_collisions", "enabled"), &VoxelLodTerrain::set_generate_collisions);

	ClassDB::bind_method(D_METHOD("get_collision_lod_count"), &VoxelLodTerrain::get_collision_lod_count);
	ClassDB::bind_method(D_METHOD("set_collision_lod_count", "count"), &VoxelLodTerrain::set_collision_lod_count);

	ClassDB::bind_method(D_METHOD("get_viewer_path"), &VoxelLodTerrain::get_viewer_path);
	ClassDB::bind_method(D_METHOD("set_viewer_path", "path"), &VoxelLodTerrain::set_viewer_path);

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("set_lod_split_scale", "lod_split_scale"), &VoxelLodTerrain::set_lod_split_scale);
	ClassDB::bind_method(D_METHOD("get_lod_split_scale"), &VoxelLodTerrain::get_lod_split_scale);

	ClassDB::bind_method(D_METHOD("get_block_region_extent"), &VoxelLodTerrain::get_block_region_extent);
	ClassDB::bind_method(D_METHOD("get_block_info", "block_pos", "lod"), &VoxelLodTerrain::get_block_info);
	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelLodTerrain::get_statistics);
	ClassDB::bind_method(D_METHOD("voxel_to_block_position", "lod_index"), &VoxelLodTerrain::voxel_to_block_position);

	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelLodTerrain::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelLodTerrain::_on_stream_params_changed);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lod_split_scale"), "set_lod_split_scale", "get_lod_split_scale");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "viewer_path"), "set_viewer_path", "get_viewer_path");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"), "set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_lod_count"), "set_collision_lod_count", "get_collision_lod_count");
}

#ifdef VOXEL_DEBUG_BOXES

void VoxelLodTerrain::create_octree_debug_box(OctreeItem &item, Vector3i pos) {
	CRASH_COND(item.debug_box != nullptr);
	MeshInstance *mi = memnew(MeshInstance);
	mi->set_mesh(VoxelDebug::get_debug_box_mesh());
	float s = 1 << (get_block_size_pow2() + get_lod_count() - 1);
	mi->set_scale(Vector3(s, s, s));
	mi->set_translation(pos.to_vec3() * s);
	add_child(mi);
	item.debug_box = mi;
}

void VoxelLodTerrain::destroy_octree_debug_box(OctreeItem &item, Vector3i pos) {
	if (item.debug_box == nullptr) {
		return;
	}
	item.debug_box->queue_delete();
	item.debug_box = nullptr;
}

#endif
