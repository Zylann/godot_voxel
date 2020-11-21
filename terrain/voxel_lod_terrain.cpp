#include "voxel_lod_terrain.h"
#include "../edition/voxel_tool_lod_terrain.h"
#include "../math/rect3i.h"
#include "../server/voxel_server.h"
#include "../streams/voxel_stream_file.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "../util/profiling_clock.h"
#include "../voxel_string_names.h"
#include "voxel_map.h"

#include <core/core_string_names.h>
#include <core/engine.h>
#include <scene/3d/mesh_instance.h>
#include <scene/resources/packed_scene.h>

namespace {

Ref<ArrayMesh> build_mesh(const Vector<Array> surfaces, Mesh::PrimitiveType primitive, int compression_flags,
		Ref<Material> material) {

	Ref<ArrayMesh> mesh;
	mesh.instance();

	unsigned int surface_index = 0;
	for (int i = 0; i < surfaces.size(); ++i) {
		Array surface = surfaces[i];

		if (surface.empty()) {
			continue;
		}

		CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(surface)) {
			continue;
		}

		mesh->add_surface_from_arrays(primitive, surface, Array(), compression_flags);
		mesh->surface_set_material(surface_index, material);
		// No multi-material supported yet
		++surface_index;
	}

	if (is_mesh_empty(mesh)) {
		mesh = Ref<Mesh>();
	}

	return mesh;
}

} // namespace

VoxelLodTerrain::VoxelLodTerrain() {
	// Note: don't do anything heavy in the constructor.
	// Godot may create and destroy dozens of instances of all node types on startup,
	// due to how ClassDB gets its default values.

	PRINT_VERBOSE("Construct VoxelLodTerrain");

	set_notify_transform(true);

	// Doing this to setup the defaults
	set_process_mode(_process_mode);

	// Infinite by default
	_bounds_in_voxels = Rect3i::from_center_extents(Vector3i(0), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT));

	_volume_id = VoxelServer::get_singleton()->add_volume(&_reception_buffers, VoxelServer::VOLUME_SPARSE_OCTREE);
	VoxelServer::get_singleton()->set_volume_octree_split_scale(_volume_id, get_lod_split_scale());

	// TODO Being able to set a LOD smaller than the stream is probably a bad idea,
	// Because it prevents edits from propagating up to the last one, they will be left out of sync
	set_lod_count(4);
	set_lod_split_scale(3);
}

VoxelLodTerrain::~VoxelLodTerrain() {
	PRINT_VERBOSE("Destroy VoxelLodTerrain");

	if (_stream.is_valid()) {
		// Schedule saving of all modified blocks,
		// without copy because we are destroying the map anyways
		save_all_modified_blocks(false);
	}

	VoxelServer::get_singleton()->remove_volume(_volume_id);
}

String VoxelLodTerrain::get_configuration_warning() const {
	if (_stream.is_valid()) {
		Ref<Script> script = _stream->get_script();
		if (script.is_valid()) {
			if (script->is_tool()) {
				// TODO This is very annoying. Probably needs an issue or proposal in Godot so we can handle this properly?
				return TTR("Be careful! Don't edit your custom stream while it's running, it can cause crashes. "
						   "Turn off `run_stream_in_editor` before doing so.");
			} else {
				return TTR("The custom stream is not tool, the editor won't be able to use it.");
			}
		}
		if (!(_stream->get_used_channels_mask() & (1 << VoxelBuffer::CHANNEL_SDF))) {
			return TTR("VoxelLodTerrain supports only stream channel \"Sdf\" (smooth), "
					   "but `get_used_channels_mask()` tells it's providing none of these.");
		}
	}
	return String();
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
	return _lods[0].map.get_block_size();
}

unsigned int VoxelLodTerrain::get_block_size_pow2() const {
	return _lods[0].map.get_block_size_pow2();
}

void VoxelLodTerrain::set_stream(Ref<VoxelStream> p_stream) {
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

void VoxelLodTerrain::_on_stream_params_changed() {
	stop_streamer();
	stop_updater();

	Ref<VoxelStreamFile> file_stream = _stream;
	if (file_stream.is_valid()) {
		const int stream_block_size_po2 = file_stream->get_block_size_po2();
		_set_block_size_po2(stream_block_size_po2);

		const int stream_lod_count = file_stream->get_lod_count();
		_set_lod_count(min(stream_lod_count, get_lod_count()));
	}

	VoxelServer::get_singleton()->set_volume_block_size(_volume_id, get_block_size());

	reset_maps();

	if (_stream.is_valid() && (!Engine::get_singleton()->is_editor_hint() || _run_stream_in_editor)) {
		start_streamer();
		start_updater();
	}

	// The whole map might change, so make all area dirty
	for (int i = 0; i < get_lod_count(); ++i) {
		Lod &lod = _lods[i];
		lod.last_view_distance_blocks = 0;
	}

	update_configuration_warning();
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

	_on_stream_params_changed();
}

void VoxelLodTerrain::_set_block_size_po2(int p_block_size_po2) {
	_lods[0].map.create(p_block_size_po2, 0);
}

// Marks intersecting blocks in the area as modified, updates LODs and schedules remeshing.
// The provided box must be at LOD0 coordinates.
void VoxelLodTerrain::post_edit_area(Rect3i p_box) {
	const Rect3i box = p_box.padded(1);
	const Rect3i bbox = box.downscaled(get_block_size());

	bbox.for_each_cell([this](Vector3i block_pos_lod0) {
		post_edit_block_lod0(block_pos_lod0);
	});
}

void VoxelLodTerrain::post_edit_block_lod0(Vector3i block_pos_lod0) {
	Lod &lod0 = _lods[0];
	VoxelBlock *block = lod0.map.get_block(block_pos_lod0);
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
	vt->set_channel(VoxelBuffer::CHANNEL_SDF);
	return Ref<VoxelTool>(vt);
}

int VoxelLodTerrain::get_view_distance() const {
	return _view_distance_voxels;
}

// TODO Needs to be clamped dynamically, to avoid the user accidentally setting blowing up memory.
// It used to be clamped to a hardcoded value, but now it may depend on LOD count and boundaries
void VoxelLodTerrain::set_view_distance(int p_distance_in_voxels) {
	ERR_FAIL_COND(p_distance_in_voxels <= 0);
	// Note: this is a hint distance, the terrain will attempt to have this radius filled with loaded voxels.
	// It is possible for blocks to still load beyond that distance.
	_view_distance_voxels = p_distance_in_voxels;
}

void VoxelLodTerrain::start_updater() {
}

void VoxelLodTerrain::stop_updater() {
	struct ResetMeshStateAction {
		void operator()(VoxelBlock *block) {
			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
			}
		}
	};

	VoxelServer::get_singleton()->invalidate_volume_mesh_requests(_volume_id);

	_reception_buffers.mesh_output.clear();

	for (unsigned int i = 0; i < _lods.size(); ++i) {
		Lod &lod = _lods[i];
		lod.blocks_pending_update.clear();

		ResetMeshStateAction a;
		lod.map.for_all_blocks(a);
	}
}

void VoxelLodTerrain::start_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, _stream);
}

void VoxelLodTerrain::stop_streamer() {
	VoxelServer::get_singleton()->set_volume_stream(_volume_id, Ref<VoxelStream>());

	for (unsigned int i = 0; i < _lods.size(); ++i) {
		Lod &lod = _lods[i];
		lod.loading_blocks.clear();
		lod.blocks_to_load.clear();
	}

	_reception_buffers.data_output.clear();
}

void VoxelLodTerrain::set_lod_split_scale(float p_lod_split_scale) {
	if (p_lod_split_scale == _lod_split_scale) {
		return;
	}

	_lod_split_scale =
			clamp(p_lod_split_scale, VoxelConstants::MINIMUM_LOD_SPLIT_SCALE, VoxelConstants::MAXIMUM_LOD_SPLIT_SCALE);

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.set_split_scale(_lod_split_scale);

		// Because `set_split_scale` may clamp it...
		_lod_split_scale = item.octree.get_split_scale();
	}

	VoxelServer::get_singleton()->set_volume_octree_split_scale(_volume_id, get_lod_split_scale());
}

float VoxelLodTerrain::get_lod_split_scale() const {
	return _lod_split_scale;
}

void VoxelLodTerrain::set_lod_count(int p_lod_count) {
	ERR_FAIL_COND(p_lod_count >= (int)VoxelConstants::MAX_LOD);
	ERR_FAIL_COND(p_lod_count < 1);

	if (get_lod_count() != p_lod_count) {
		_set_lod_count(p_lod_count);
	}
}

void VoxelLodTerrain::_set_lod_count(int p_lod_count) {
	CRASH_COND(p_lod_count >= (int)VoxelConstants::MAX_LOD);
	CRASH_COND(p_lod_count < 1);

	_lod_count = p_lod_count;

	LodOctree::NoDestroyAction nda;

	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		OctreeItem &item = E->value();
		item.octree.create_from_lod_count(get_block_size(), p_lod_count, nda);
	}

	// Not entirely required, but changing LOD count at runtime is rarely needed
	reset_maps();
}

void VoxelLodTerrain::reset_maps() {
	// Clears all blocks and reconfigures maps to account for new LOD count and block sizes

	// Don't reset while streaming, the result can be dirty?
	//CRASH_COND(_stream_thread != nullptr);

	for (int lod_index = 0; lod_index < (int)_lods.size(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		// Instance new maps if we have more lods, or clear them otherwise
		if (lod_index < get_lod_count()) {
			lod.map.create(get_block_size_pow2(), lod_index);
			lod.blocks_to_load.clear();
			lod.last_view_distance_blocks = 0;

		} else {
			lod.map.clear();
		}
	}

	// Reset previous state caches to force rebuilding the view area
	_last_octree_region_box = Rect3i();
	_lod_octrees.clear();
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

int VoxelLodTerrain::get_block_region_extent() const {
	return VoxelServer::get_octree_lod_block_region_extent(_lod_split_scale);
}

Vector3 VoxelLodTerrain::voxel_to_block_position(Vector3 vpos, int lod_index) const {
	ERR_FAIL_COND_V(lod_index < 0, Vector3());
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), Vector3());
	const Lod &lod = _lods[lod_index];
	Vector3i bpos = lod.map.voxel_to_block(Vector3i(vpos)) >> lod_index;
	return bpos.to_vec3();
}

void VoxelLodTerrain::set_process_mode(ProcessMode mode) {
	_process_mode = mode;
	set_process(_process_mode == PROCESS_MODE_IDLE);
	set_physics_process(_process_mode == PROCESS_MODE_PHYSICS);
}

void VoxelLodTerrain::_notification(int p_what) {
	switch (p_what) {
		// TODO Should use NOTIFICATION_INTERNAL_PROCESS instead?
		case NOTIFICATION_PROCESS:
			if (_process_mode == PROCESS_MODE_IDLE) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelServerUpdater::ensure_existence(get_tree());
				_process();
			}
			break;

		// TODO Should use NOTIFICATION_INTERNAL_PHYSICS_PROCESS instead?
		case NOTIFICATION_PHYSICS_PROCESS:
			if (_process_mode == PROCESS_MODE_PHYSICS) {
				// Can't do that in enter tree because Godot is "still setting up children".
				// Can't do that in ready either because Godot says node state is locked.
				// This hack is quite miserable.
				VoxelServerUpdater::ensure_existence(get_tree());
				_process();
				break;
			}

		case NOTIFICATION_EXIT_TREE:
			break;

		case NOTIFICATION_ENTER_WORLD: {
			World *world = *get_world();
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].map.for_all_blocks([world](VoxelBlock *block) {
					block->set_world(world);
				});
			}
#ifdef TOOLS_ENABLED
			if (is_showing_gizmos()) {
				_debug_renderer.set_world(is_visible_in_tree() ? world : nullptr);
			}
#endif
		} break;

		case NOTIFICATION_EXIT_WORLD: {
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].map.for_all_blocks([](VoxelBlock *block) {
					block->set_world(nullptr);
				});
			}
#ifdef TOOLS_ENABLED
			_debug_renderer.set_world(nullptr);
#endif
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			const bool visible = is_visible();
			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].map.for_all_blocks([visible](VoxelBlock *block) {
					block->set_parent_visible(visible);
				});
			}
#ifdef TOOLS_ENABLED
			if (is_showing_gizmos()) {
				_debug_renderer.set_world(is_visible_in_tree() ? *get_world() : nullptr);
			}
#endif
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			VOXEL_PROFILE_SCOPE_NAMED("VoxelLodTerrain::NOTIFICATION_TRANSFORM_CHANGED");

			const Transform transform = get_global_transform();
			VoxelServer::get_singleton()->set_volume_transform(_volume_id, transform);

			if (!is_inside_tree()) {
				// The transform and other properties can be set by the scene loader,
				// before we enter the tree
				return;
			}

			for (unsigned int lod_index = 0; lod_index < _lods.size(); ++lod_index) {
				_lods[lod_index].map.for_all_blocks([&transform](VoxelBlock *block) {
					block->set_parent_transform(transform);
				});
			}
		} break;

		default:
			break;
	}
}

Vector3 VoxelLodTerrain::get_local_viewer_pos() const {
	if (Engine::get_singleton()->is_editor_hint()) {
		// TODO Use editor's camera here
		return Vector3();

	} else {
		Vector3 pos = (_lods[0].last_viewer_block_pos << _lods[0].map.get_block_size_pow2()).to_vec3();

		// TODO Support for multiple viewers, this is a placeholder implementation
		VoxelServer::get_singleton()->for_each_viewer([&pos](const VoxelServer::Viewer &viewer, uint32_t viewer_id) {
			pos = viewer.world_position;
		});

		const Transform world_to_local = get_global_transform().affine_inverse();
		pos = world_to_local.xform(pos);
		return pos;
	}
}

void VoxelLodTerrain::try_schedule_loading_with_neighbors(const Vector3i &p_bpos, int lod_index) {
	Lod &lod = _lods[lod_index];

	const int p = lod.map.get_block_size_pow2() + lod_index;

	const int bound_min_x = _bounds_in_voxels.pos.x >> p;
	const int bound_min_y = _bounds_in_voxels.pos.y >> p;
	const int bound_min_z = _bounds_in_voxels.pos.z >> p;
	const int bound_max_x = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_y = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_z = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;

	const int min_x = max(p_bpos.x - 1, bound_min_x);
	const int min_y = max(p_bpos.y - 1, bound_min_y);
	const int min_z = max(p_bpos.z - 1, bound_min_z);
	const int max_x = min(p_bpos.x + 2, bound_max_x);
	const int max_y = min(p_bpos.y + 2, bound_max_y);
	const int max_z = min(p_bpos.z + 2, bound_max_z);

	Vector3i bpos;
	for (bpos.y = min_y; bpos.y < max_y; ++bpos.y) {
		for (bpos.z = min_z; bpos.z < max_z; ++bpos.z) {
			for (bpos.x = min_x; bpos.x < max_x; ++bpos.x) {
				VoxelBlock *block = lod.map.get_block(bpos);

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

bool VoxelLodTerrain::is_block_surrounded(const Vector3i &p_bpos, int lod_index, const VoxelMap &map) const {
	const int p = map.get_block_size_pow2() + lod_index;

	const int bound_min_x = _bounds_in_voxels.pos.x >> p;
	const int bound_min_y = _bounds_in_voxels.pos.y >> p;
	const int bound_min_z = _bounds_in_voxels.pos.z >> p;
	const int bound_max_x = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_y = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;
	const int bound_max_z = (_bounds_in_voxels.pos.x + _bounds_in_voxels.size.x) >> p;

	const int min_x = max(p_bpos.x - 1, bound_min_x);
	const int min_y = max(p_bpos.y - 1, bound_min_y);
	const int min_z = max(p_bpos.z - 1, bound_min_z);
	const int max_x = min(p_bpos.x + 2, bound_max_x);
	const int max_y = min(p_bpos.y + 2, bound_max_y);
	const int max_z = min(p_bpos.z + 2, bound_max_z);

	Vector3i bpos;
	for (bpos.y = min_y; bpos.y < max_y; ++bpos.y) {
		for (bpos.z = min_z; bpos.z < max_z; ++bpos.z) {
			for (bpos.x = min_x; bpos.x < max_x; ++bpos.x) {
				if (bpos != p_bpos && !map.has_block(bpos)) {
					return false;
				}
			}
		}
	}
	return true;
}

bool VoxelLodTerrain::check_block_loaded_and_updated(const Vector3i &p_bpos, int lod_index) {
	Lod &lod = _lods[lod_index];
	VoxelBlock *block = lod.map.get_block(p_bpos);
	if (block == nullptr) {
		try_schedule_loading_with_neighbors(p_bpos, lod_index);
		return false;
	}
	return check_block_mesh_updated(block);
}

bool VoxelLodTerrain::check_block_mesh_updated(VoxelBlock *block) {
	VOXEL_PROFILE_SCOPE();
	CRASH_COND(block == nullptr);
	Lod &lod = _lods[block->lod_index];

	switch (block->get_mesh_state()) {
		case VoxelBlock::MESH_NEVER_UPDATED:
		case VoxelBlock::MESH_NEED_UPDATE:
			if (is_block_surrounded(block->position, block->lod_index, lod.map)) {
				lod.blocks_pending_update.push_back(block->position);
				block->set_mesh_state(VoxelBlock::MESH_UPDATE_NOT_SENT);
			} else {
				try_schedule_loading_with_neighbors(block->position, block->lod_index);
			}
			return false;

		case VoxelBlock::MESH_UPDATE_NOT_SENT:
		case VoxelBlock::MESH_UPDATE_SENT:
			return false;

		case VoxelBlock::MESH_UP_TO_DATE:
			return true;

		default:
			CRASH_NOW();
			break;
	}

	return true;
}

void VoxelLodTerrain::send_block_data_requests() {
	// Blocks to load
	for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
		Lod &lod = _lods[lod_index];

		for (unsigned int i = 0; i < lod.blocks_to_load.size(); ++i) {
			const Vector3i block_pos = lod.blocks_to_load[i];
			VoxelServer::get_singleton()->request_block_load(_volume_id, block_pos, lod_index);
		}

		lod.blocks_to_load.clear();
	}

	// Blocks to save
	for (unsigned int i = 0; i < _blocks_to_save.size(); ++i) {
		PRINT_VERBOSE(String("Requesting save of block {0} lod {1}")
							  .format(varray(_blocks_to_save[i].position.to_vec3(), _blocks_to_save[i].lod)));
		const BlockToSave &b = _blocks_to_save[i];
		VoxelServer::get_singleton()->request_block_save(_volume_id, b.voxels, b.position, b.lod);
	}

	_blocks_to_save.clear();
}

void VoxelLodTerrain::_process() {
	VOXEL_PROFILE_SCOPE();

	if (get_lod_count() == 0) {
		// If there isn't a LOD 0, there is nothing to load
		return;
	}

	// Get viewer location in voxel space
	const Vector3 viewer_pos = get_local_viewer_pos();

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
		VOXEL_PROFILE_SCOPE();
		// TODO Could it actually be enough to have a rolling update on all blocks?

		// This should be the same distance relatively to each LOD
		const int block_region_extent = get_block_region_extent();

		// Ignore largest lod because it can extend a little beyond due to the view distance setting.
		// Instead, those blocks are unloaded by the octree forest management.
		// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
		for (int lod_index = get_lod_count() - 2; lod_index >= 0; --lod_index) {
			VOXEL_PROFILE_SCOPE();
			Lod &lod = _lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			unsigned int block_size_po2 = _lods[0].map.get_block_size_pow2() + lod_index;
			Vector3i viewer_block_pos_within_lod = VoxelMap::voxel_to_block_b(viewer_pos, block_size_po2);

			const Rect3i bounds_in_blocks = Rect3i(
					_bounds_in_voxels.pos >> block_size_po2,
					_bounds_in_voxels.size >> block_size_po2);

			const Rect3i new_box =
					Rect3i::from_center_extents(viewer_block_pos_within_lod, Vector3i(block_region_extent));
			const Rect3i prev_box =
					Rect3i::from_center_extents(lod.last_viewer_block_pos, Vector3i(lod.last_view_distance_blocks));

			if (!new_box.intersects(bounds_in_blocks) && !prev_box.intersects(bounds_in_blocks)) {
				// If this box doesn't intersect either now or before, there is no chance a smaller one will
				break;
			}

			// Eliminate pending blocks that aren't needed

			// This vector must be empty at this point.
			// Let's assert so it will pop on your face the day that assumption changes
			CRASH_COND(!lod.blocks_to_load.empty());

			if (prev_box != new_box) {
				VOXEL_PROFILE_SCOPE();
				prev_box.difference(new_box, [this, lod_index](Rect3i out_of_range_box) {
					out_of_range_box.for_each_cell([=](Vector3i pos) {
						//print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
						immerge_block(pos, lod_index);
					});
				});
			}

			// Cancel block updates that are not within the padded region (since neighbors are always required to remesh)
			const Rect3i padded_new_box = new_box.padded(-1);
			{
				VOXEL_PROFILE_SCOPE();
				unordered_remove_if(lod.blocks_pending_update, [&lod, padded_new_box](Vector3i bpos) {
					if (padded_new_box.contains(bpos)) {
						return false;
					} else {
						VoxelBlock *block = lod.map.get_block(bpos);
						if (block != nullptr) {
							block->set_mesh_state(VoxelBlock::MESH_NEED_UPDATE);
						}
						return true;
					}
				});
			}

			lod.last_viewer_block_pos = viewer_block_pos_within_lod;
			lod.last_view_distance_blocks = block_region_extent;
		}
	}

	// Create and remove octrees in a grid around the viewer
	{
		VOXEL_PROFILE_SCOPE();
		// TODO Investigate if multi-octree can produce cracks in the terrain (so far I haven't noticed)

		const unsigned int octree_size_po2 = get_block_size_pow2() + get_lod_count() - 1;
		const unsigned int octree_size = 1 << octree_size_po2;
		const unsigned int octree_region_extent = 1 + _view_distance_voxels / (1 << octree_size_po2);

		const Vector3i viewer_octree_pos = (Vector3i(viewer_pos) + Vector3i(octree_size / 2)) >> octree_size_po2;

		const Rect3i bounds_in_octrees = _bounds_in_voxels.downscaled(octree_size);

		const Rect3i new_box = Rect3i::from_center_extents(viewer_octree_pos, Vector3i(octree_region_extent))
									   .clipped(bounds_in_octrees);
		const Rect3i prev_box = _last_octree_region_box;

		if (new_box != prev_box) {
			VOXEL_PROFILE_SCOPE();

			struct CleanOctreeAction {
				VoxelLodTerrain *self;
				Vector3i block_offset_lod0;

				void operator()(Vector3i node_pos, unsigned int lod_index) {
					Lod &lod = self->_lods[lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

					VoxelBlock *block = lod.map.get_block(bpos);
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

					int last_lod_index = self->get_lod_count() - 1;

					// We just drop the octree and hide blocks it was considering as visible.
					// Normally such octrees shouldn't bee too deep as they will likely be at the edge
					// of the loaded area, unless the player teleported far away.
					CleanOctreeAction a;
					a.self = self;
					a.block_offset_lod0 = block_pos_maxlod << last_lod_index;
					item.octree.clear(a);

					self->_lod_octrees.erase(E);

					// Immerge last lod from here, as it may extend a bit further than the others
					self->immerge_block(pos, last_lod_index);
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
					LodOctree::NoDestroyAction nda;
					item.octree.create_from_lod_count(block_size, self->get_lod_count(), nda);
					item.octree.set_split_scale(self->_lod_split_scale);
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

	CRASH_COND(_blocks_pending_transition_update.size() != 0);

	// Find which blocks we need to load and see, within each octree
	{
		VOXEL_PROFILE_SCOPE();

		// TODO Maintain a vector to make iteration faster?
		for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
			VOXEL_PROFILE_SCOPE();

			OctreeItem &item = E->value();
			Vector3i block_pos_maxlod = E->key();
			Vector3i block_offset_lod0 = block_pos_maxlod << (get_lod_count() - 1);

			struct OctreeActions {
				VoxelLodTerrain *self = nullptr;
				Vector3i block_offset_lod0;
				unsigned int blocked_count = 0;

				void create_child(Vector3i node_pos, int lod_index) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelBlock *block = lod.map.get_block(bpos);

					// Never show a child that hasn't been meshed
					CRASH_COND(block == nullptr);
					CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UP_TO_DATE);

					block->set_visible(true);
					self->add_transition_update(block);
					self->add_transition_updates_around(bpos, lod_index);
				}

				void destroy_child(Vector3i node_pos, int lod_index) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelBlock *block = lod.map.get_block(bpos);

					if (block) {
						block->set_visible(false);
						self->add_transition_updates_around(bpos, lod_index);
					}
				}

				void show_parent(Vector3i node_pos, int lod_index) {
					Lod &lod = self->_lods[lod_index];
					Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
					VoxelBlock *block = lod.map.get_block(bpos);

					// If we teleport far away, the area we were in is going to merge,
					// and blocks may have been unloaded completely.
					// So in that case it's normal to not find any block.
					// Otherwise, there must always be a visible parent in the end, unless the octree vanished.
					if (block != nullptr && block->get_mesh_state() == VoxelBlock::MESH_UP_TO_DATE) {
						block->set_visible(true);
						self->add_transition_update(block);
						self->add_transition_updates_around(bpos, lod_index);
					}
				}

				void hide_parent(Vector3i node_pos, int lod_index) {
					destroy_child(node_pos, lod_index); // Same
				}

				bool can_create_root(int lod_index) {
					Vector3i offset = block_offset_lod0 >> lod_index;
					return self->check_block_loaded_and_updated(offset, lod_index);
				}

				bool can_split(Vector3i node_pos, int child_lod_index) {
					Vector3i offset = block_offset_lod0 >> child_lod_index;
					bool can = true;

					// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
					for (int i = 0; i < 8; ++i) {

						// Get block pos local-to-region
						Vector3i child_pos = LodOctree::get_child_position(node_pos, i);

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

				bool can_join(Vector3i node_pos, int parent_lod_index) {
					// Can only unsubdivide if the parent mesh is ready
					Lod &lod = self->_lods[parent_lod_index];

					Vector3i bpos = node_pos + (block_offset_lod0 >> parent_lod_index);
					VoxelBlock *block = lod.map.get_block(bpos);

					if (block == nullptr) {
						// The block got unloaded. Exceptionally, we can join.
						// There will always be a grand-parent because we never destroy them when they split,
						// and we never create a child without creating a parent first.
						return true;
					}

					// The block is loaded but the mesh isn't up to date, we need to ping and wait.
					bool can = self->check_block_mesh_updated(block);

					if (!can) {
						++blocked_count;
					}

					return can;
				}
			};

			OctreeActions octree_actions;
			octree_actions.self = this;
			octree_actions.block_offset_lod0 = block_offset_lod0;

			Vector3 relative_viewer_pos = viewer_pos - get_block_size() * block_offset_lod0.to_vec3();
			item.octree.update(relative_viewer_pos, octree_actions);

			// Ideally, this stat should stabilize to zero.
			// If not, something in block management prevents LODs from properly show up and should be fixed.
			_stats.blocked_lods += octree_actions.blocked_count;
		}

		{
			VOXEL_PROFILE_SCOPE();
			process_transition_updates();
		}
	}

	CRASH_COND(_blocks_pending_transition_update.size() != 0);

	_stats.time_detect_required_blocks = profiling_clock.restart();

	// It's possible the user didn't set a stream yet, or it is turned off
	if (_stream.is_valid() && (Engine::get_singleton()->is_editor_hint() == false || _run_stream_in_editor)) {
		send_block_data_requests();
	}

	_stats.time_request_blocks_to_load = profiling_clock.restart();

	// Get block loading responses
	// Note: if block loading is too fast, this can cause stutters.
	// It should only happen on first load, though.
	{
		VOXEL_PROFILE_SCOPE();

		for (size_t reception_index = 0; reception_index < _reception_buffers.data_output.size(); ++reception_index) {
			VOXEL_PROFILE_SCOPE();
			const VoxelServer::BlockDataOutput &ob = _reception_buffers.data_output[reception_index];

			if (ob.type == VoxelServer::BlockDataOutput::TYPE_SAVE) {
				// That's a save confirmation event.
				// Note: in the future, if blocks don't get copied before being sent for saving,
				// we will need to use block versionning to know when we can reset the `modified` flag properly

				// TODO Now that's the case. Use version? Or just keep copying?
				continue;
			}

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

			if (ob.dropped) {
				// That block was dropped by the data loader thread, but we were still expecting it...
				// This is most likely caused by the loader not keeping up with the speed at which the player is moving.
				// We should recover with the removal from `loading_blocks` so it will be re-queried again later...

				//				print_line(String("Received a block loading drop while we were still expecting it: lod{0} ({1}, {2}, {3})")
				//								   .format(varray(ob.lod, ob.position.x, ob.position.y, ob.position.z)));

				++_stats.dropped_block_loads;
				continue;
			}

			if (ob.voxels->get_size() != Vector3i(lod.map.get_block_size())) {
				// Voxel block size is incorrect, drop it
				ERR_PRINT("Block size obtained from stream is different from expected size");
				++_stats.dropped_block_loads;
				continue;
			}

			// Store buffer
			VoxelBlock *block = lod.map.set_block_buffer(ob.position, ob.voxels);
			//print_line(String("Adding block {0} at lod {1}").format(varray(eo.block_position.to_vec3(), eo.lod)));
			// The block will be made visible and meshed only by LodOctree
			block->set_visible(false);
			block->set_parent_visible(is_visible());
			block->set_world(get_world());

			Ref<ShaderMaterial> shader_material = _material;
			if (shader_material.is_valid() && block->get_shader_material().is_null()) {
				VOXEL_PROFILE_SCOPE();

				// Pooling shader materials is necessary for now, to avoid stuttering in the editor.
				// Due to a signal used to keep the inspector up to date, even though these
				// material copies will never be seen in the inspector
				// See https://github.com/godotengine/godot/issues/34741
				Ref<ShaderMaterial> sm;
				if (_shader_material_pool.size() > 0) {
					sm = _shader_material_pool.back();
					// The joys of pooling materials
					sm->set_shader_param(VoxelStringNames::get_singleton()->u_transition_mask, 0);
					_shader_material_pool.pop_back();
				} else {
					sm = shader_material->duplicate(false);
				}

				// Set individual shader material, because each block can have dynamic parameters,
				// used to smooth seams without re-uploading meshes and allow to implement LOD fading
				block->set_shader_material(sm);
			}
		}
	}

	_stats.time_process_load_responses = profiling_clock.restart();

	// Send mesh updates
	{
		VOXEL_PROFILE_SCOPE();

		for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
			VOXEL_PROFILE_SCOPE();
			Lod &lod = _lods[lod_index];

			for (unsigned int bi = 0; bi < lod.blocks_pending_update.size(); ++bi) {
				VOXEL_PROFILE_SCOPE();
				const Vector3i block_pos = lod.blocks_pending_update[bi];

				VoxelBlock *block = lod.map.get_block(block_pos);
				CRASH_COND(block == nullptr);
				// All blocks we get here must be in the scheduled state
				CRASH_COND(block->get_mesh_state() != VoxelBlock::MESH_UPDATE_NOT_SENT);

				// Get block and its neighbors
				VoxelServer::BlockMeshInput mesh_request;
				mesh_request.position = block_pos;
				mesh_request.lod = lod_index;
				for (unsigned int i = 0; i < Cube::MOORE_AREA_3D_COUNT; ++i) {
					const Vector3i npos = block_pos + Cube::g_ordered_moore_area_3d[i];
					VoxelBlock *nblock = lod.map.get_block(npos);
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

			lod.blocks_pending_update.clear();
		}
	}

	_stats.time_request_blocks_to_update = profiling_clock.restart();

	// Receive mesh updates
	{
		VOXEL_PROFILE_SCOPE_NAMED("Receive mesh updates");

		// Allocate milliseconds max to upload meshes
		const OS &os = *OS::get_singleton();
		const uint32_t timeout = os.get_ticks_msec() + VoxelConstants::MAIN_THREAD_MESHING_BUDGET_MS;

		const Transform global_transform = get_global_transform();

		// The following is done on the main thread because Godot doesn't really support multithreaded Mesh allocation.
		// This also proved to be very slow compared to the meshing process itself...
		// hopefully Vulkan will allow us to upload graphical resources without stalling rendering as they upload?

		size_t queue_index = 0;
		for (; queue_index < _reception_buffers.mesh_output.size() && os.get_ticks_msec() < timeout; ++queue_index) {
			VOXEL_PROFILE_SCOPE();
			const VoxelServer::BlockMeshOutput &ob = _reception_buffers.mesh_output[queue_index];

			if (ob.lod >= get_lod_count()) {
				// Sorry, LOD configuration changed, drop that mesh
				++_stats.dropped_block_meshs;
				continue;
			}

			Lod &lod = _lods[ob.lod];

			VoxelBlock *block = lod.map.get_block(ob.position);
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

			if (block->get_mesh_state() == VoxelBlock::MESH_UPDATE_SENT) {
				block->set_mesh_state(VoxelBlock::MESH_UP_TO_DATE);
			}

			const VoxelMesher::Output mesh_data = ob.smooth_surfaces;

			Ref<ArrayMesh> mesh = build_mesh(
					mesh_data.surfaces,
					mesh_data.primitive_type,
					mesh_data.compression_flags,
					_material);

			bool has_collision = _generate_collisions;
			if (has_collision && _collision_lod_count != -1) {
				has_collision = ob.lod < _collision_lod_count;
			}

			block->set_mesh(mesh);
			if (has_collision) {
				block->set_collision_mesh(mesh_data.surfaces, get_tree()->is_debugging_collisions_hint(), this);
			}

			{
				VOXEL_PROFILE_SCOPE();
				for (unsigned int dir = 0; dir < mesh_data.transition_surfaces.size(); ++dir) {

					Ref<ArrayMesh> transition_mesh = build_mesh(
							mesh_data.transition_surfaces[dir],
							mesh_data.primitive_type,
							mesh_data.compression_flags,
							_material);

					block->set_transition_mesh(transition_mesh, dir);
				}
			}

			block->set_parent_transform(global_transform);
		}

		{
			VOXEL_PROFILE_SCOPE();
			shift_up(_reception_buffers.mesh_output, queue_index);
		}
	}

	_stats.time_process_update_responses = profiling_clock.restart();

#ifdef TOOLS_ENABLED
	if (is_showing_gizmos()) {
		update_gizmos();
	}
#endif
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
		const Vector3i bpos = lod0.blocks_pending_lodding[i];
		VoxelBlock *block = lod0.map.get_block(bpos);
		block->set_needs_lodding(false);
		L::schedule_update(block, lod0.blocks_pending_update);
	}

	const int half_bs = get_block_size() >> 1;

	// Process downscales upwards in pairs of consecutive LODs.
	// This ensures we don't process multiple times the same blocks.
	// Only LOD0 is editable at the moment, so we'll downscale from there
	for (int dst_lod_index = 1; dst_lod_index < _lod_count; ++dst_lod_index) {
		Lod &src_lod = _lods[dst_lod_index - 1];
		Lod &dst_lod = _lods[dst_lod_index];

		for (unsigned int i = 0; i < src_lod.blocks_pending_lodding.size(); ++i) {
			const Vector3i src_bpos = src_lod.blocks_pending_lodding[i];
			const Vector3i dst_bpos = src_bpos >> 1;

			VoxelBlock *src_block = src_lod.map.get_block(src_bpos);
			VoxelBlock *dst_block = dst_lod.map.get_block(dst_bpos);

			src_block->set_needs_lodding(false);

			if (dst_block == nullptr) {
				ERR_PRINT(String("Destination block {0} not found when cascading edits on LOD {1}")
								  .format(varray(dst_bpos.to_vec3(), dst_lod_index)));
				continue;
			}

			// The block and its lower LODs are expected to be available.
			// Otherwise it means the function was called too late
			CRASH_COND(src_block == nullptr);
			//CRASH_COND(dst_block == nullptr);
			CRASH_COND(src_block->voxels.is_null());
			CRASH_COND(dst_block->voxels.is_null());

			L::schedule_update(dst_block, dst_lod.blocks_pending_update);

			dst_block->set_modified(true);

			if (dst_lod_index != _lod_count - 1 && !dst_block->get_needs_lodding()) {
				dst_block->set_needs_lodding(true);
				dst_lod.blocks_pending_lodding.push_back(dst_bpos);
			}

			const Vector3i rel = src_bpos - (dst_bpos << 1);

			// Update lower LOD
			// This must always be done after an edit before it gets saved, otherwise LODs won't match and it will look ugly.
			// TODO Try to narrow to edited region instead of taking whole block
			{
				RWLockWrite lock(src_block->voxels->get_lock());
				src_block->voxels->downscale_to(
						**dst_block->voxels, Vector3i(), src_block->voxels->get_size(), rel * half_bs);
			}
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
	std::vector<VoxelLodTerrain::BlockToSave> &blocks_to_save;
	std::vector<Ref<ShaderMaterial> > &shader_materials;
	bool with_copy;

	void operator()(VoxelBlock *block) {
		Ref<ShaderMaterial> sm = block->get_shader_material();
		if (sm.is_valid()) {
			// Recycle material
			shader_materials.push_back(sm);
			block->set_shader_material(Ref<ShaderMaterial>());
		}

		// TODO Don't ask for save if the stream doesn't support it!
		if (block->is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelLodTerrain::BlockToSave b;

			if (with_copy) {
				RWLockRead lock(block->voxels->get_lock());
				b.voxels = block->voxels->duplicate(true);
			} else {
				b.voxels = block->voxels;
			}

			b.position = block->position;
			b.lod = block->lod_index;
			blocks_to_save.push_back(b);
			block->set_modified(false);
		}
	}
};
} // namespace

void VoxelLodTerrain::immerge_block(Vector3i block_pos, int lod_index) {
	VOXEL_PROFILE_SCOPE();

	ERR_FAIL_COND(lod_index >= get_lod_count());

	Lod &lod = _lods[lod_index];

	lod.map.remove_block(block_pos, ScheduleSaveAction{ _blocks_to_save, _shader_material_pool, false });

	lod.loading_blocks.erase(block_pos);

	// Blocks in the update queue will be cancelled in _process,
	// because it's too expensive to linear-search all blocks for each block

	// No need to remove things from blocks_pending_load,
	// This vector is filled and cleared immediately in the main process.
	// It is a member only to re-use its capacity memory over frames.
}

void VoxelLodTerrain::save_all_modified_blocks(bool with_copy) {
	flush_pending_lod_edits();

	for (int i = 0; i < _lod_count; ++i) {
		// That may cause a stutter, so should be used when the player won't notice
		_lods[i].map.for_all_blocks(ScheduleSaveAction{ _blocks_to_save, _shader_material_pool, with_copy });
	}

	// And flush immediately
	send_block_data_requests();
}

void VoxelLodTerrain::add_transition_update(VoxelBlock *block) {
	if (!block->pending_transition_update) {
		_blocks_pending_transition_update.push_back(block);
		block->pending_transition_update = true;
	}
}

void VoxelLodTerrain::add_transition_updates_around(Vector3i block_pos, int lod_index) {
	Lod &lod = _lods[lod_index];

	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		Vector3i npos = block_pos + Cube::g_side_normals[dir];
		VoxelBlock *nblock = lod.map.get_block(npos);

		if (nblock != nullptr) {
			add_transition_update(nblock);
		}
	}
	// TODO If a block appears at lod, neighbor blocks at lod-1 need to be updated.
	// or maybe get_transition_mask needs a different approach that also looks at higher lods?
}

void VoxelLodTerrain::process_transition_updates() {
	for (unsigned int i = 0; i < _blocks_pending_transition_update.size(); ++i) {
		VoxelBlock *block = _blocks_pending_transition_update[i];
		CRASH_COND(block == nullptr);

		if (block->is_visible()) {
			block->set_transition_mask(get_transition_mask(block->position, block->lod_index));
		}

		block->pending_transition_update = false;
	}

	_blocks_pending_transition_update.clear();
}

uint8_t VoxelLodTerrain::get_transition_mask(Vector3i block_pos, int lod_index) const {
	uint8_t transition_mask = 0;

	if (lod_index + 1 >= _lod_count) {
		return transition_mask;
	}

	const Lod &lower_lod = _lods[lod_index + 1];
	const Lod &lod = _lods[lod_index];

	const Vector3i lower_pos = block_pos >> 1;
	const Vector3i upper_pos = block_pos << 1;

	// Based on octree rules, and the fact it must have run before, check neighbor blocks of same LOD:
	// If one is missing or not visible, it means either of the following:
	// - The neighbor at lod+1 is visible or not loaded (there must be a transition)
	// - The neighbor at lod-1 is visible (no transition)

	uint8_t visible_neighbors_of_same_lod = 0;
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		Vector3i npos = block_pos + Cube::g_side_normals[dir];

		const VoxelBlock *nblock = lod.map.get_block(npos);

		if (nblock != nullptr && nblock->is_visible()) {
			visible_neighbors_of_same_lod |= (1 << dir);
		}
	}

	if (visible_neighbors_of_same_lod != 0b111111) {
		// At least one neighbor isn't visible.
		// Check for neighbors at different LOD (there can be only one kind on a given side)
		for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			int dir_mask = (1 << dir);

			if (visible_neighbors_of_same_lod & dir_mask) {
				continue;
			}

			const Vector3i side_normal = Cube::g_side_normals[dir];
			const Vector3i lower_neighbor_pos = (block_pos + side_normal) >> 1;

			if (lower_neighbor_pos != lower_pos) {
				const VoxelBlock *lower_neighbor_block = lower_lod.map.get_block(lower_neighbor_pos);

				if (lower_neighbor_block != nullptr && lower_neighbor_block->is_visible()) {
					// The block has a visible neighbor of lower LOD
					transition_mask |= dir_mask;
					continue;
				}
			}

			if (lod_index > 0) {
				// Check upper LOD neighbors.
				// There are always 4 on each side, checking any is enough

				Vector3i upper_neighbor_pos = upper_pos;
				for (unsigned int i = 0; i < Vector3i::AXIS_COUNT; ++i) {
					if (side_normal[i] == -1) {
						--upper_neighbor_pos[i];
					} else if (side_normal[i] == 1) {
						upper_neighbor_pos[i] += 2;
					}
				}

				const Lod &upper_lod = _lods[lod_index - 1];
				const VoxelBlock *upper_neighbor_block = upper_lod.map.get_block(upper_neighbor_pos);

				if (upper_neighbor_block == nullptr || upper_neighbor_block->is_visible() == false) {
					// The block has no visible neighbor yet. World border? Assume lower LOD.
					transition_mask |= dir_mask;
				}
			}
		}
	}

	return transition_mask;
}

Dictionary VoxelLodTerrain::get_statistics() const {
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
	d["blocked_lods"] = _stats.blocked_lods;

	return d;
}

void VoxelLodTerrain::set_run_stream_in_editor(bool enable) {
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

bool VoxelLodTerrain::is_stream_running_in_editor() const {
	return _run_stream_in_editor;
}

void VoxelLodTerrain::restart_stream() {
	_on_stream_params_changed();
}

void VoxelLodTerrain::set_voxel_bounds(Rect3i p_box) {
	_bounds_in_voxels =
			p_box.clipped(Rect3i::from_center_extents(Vector3i(), Vector3i(VoxelConstants::MAX_VOLUME_EXTENT)));
	// Round to octree size
	const int octree_size = get_block_size() << (get_lod_count() - 1);
	_bounds_in_voxels = _bounds_in_voxels.snapped(octree_size);
	// Can't have a smaller region than one octree
	for (unsigned i = 0; i < Vector3i::AXIS_COUNT; ++i) {
		if (_bounds_in_voxels.size[i] < octree_size) {
			_bounds_in_voxels.size[i] = octree_size;
		}
	}
}

void VoxelLodTerrain::_b_save_modified_blocks() {
	save_all_modified_blocks(true);
}

void VoxelLodTerrain::_b_set_voxel_bounds(AABB aabb) {
	// TODO Please Godot, have an integer AABB!
	set_voxel_bounds(Rect3i(aabb.position.round(), aabb.size.round()));
}

AABB VoxelLodTerrain::_b_get_voxel_bounds() const {
	const Rect3i b = get_voxel_bounds();
	return AABB(b.pos.to_vec3(), b.size.to_vec3());
}

// DEBUG LAND

Array VoxelLodTerrain::debug_raycast_block(Vector3 world_origin, Vector3 world_direction) const {
	const Transform world_to_local = get_global_transform().affine_inverse();
	Vector3 pos = world_to_local.xform(world_origin);
	const Vector3 dir = world_to_local.basis.xform(world_direction);
	const float max_distance = 256;
	const float step = 2.f;
	float distance = 0.f;

	Array hits;
	while (distance < max_distance && hits.size() == 0) {
		for (int lod_index = 0; lod_index < _lod_count; ++lod_index) {
			const Lod &lod = _lods[lod_index];
			Vector3i bpos = lod.map.voxel_to_block(Vector3i(pos)) >> lod_index;
			const VoxelBlock *block = lod.map.get_block(bpos);
			if (block != nullptr && block->is_visible() && block->has_mesh()) {
				Dictionary d;
				d["position"] = block->position.to_vec3();
				d["lod"] = block->lod_index;
				hits.append(d);
			}
		}
		distance += step;
		pos += dir * step;
	}

	return hits;
}

Dictionary VoxelLodTerrain::debug_get_block_info(Vector3 fbpos, int lod_index) const {
	Dictionary d;
	ERR_FAIL_COND_V(lod_index < 0, d);
	ERR_FAIL_COND_V(lod_index >= get_lod_count(), d);

	const Lod &lod = _lods[lod_index];
	Vector3i bpos(fbpos);

	bool meshed = false;
	bool visible = false;
	int loading_state = 0;
	const VoxelBlock *block = lod.map.get_block(bpos);

	if (block) {
		meshed = !block->has_mesh() && block->get_mesh_state() != VoxelBlock::MESH_UP_TO_DATE;
		visible = block->is_visible();
		loading_state = 2;
		d["transition_mask"] = block->get_transition_mask();
		d["recomputed_transition_mask"] = get_transition_mask(block->position, block->lod_index);

	} else if (lod.loading_blocks.has(bpos)) {
		loading_state = 1;
	}

	d["loading"] = loading_state;
	d["meshed"] = meshed;
	d["visible"] = visible;
	return d;
}

Array VoxelLodTerrain::debug_get_octrees() const {
	Array positions;
	positions.resize(_lod_octrees.size());
	int i = 0;
	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		positions[i++] = E->key().to_vec3();
	}
	return positions;
}

#ifdef TOOLS_ENABLED

void VoxelLodTerrain::update_gizmos() {
	VOXEL_PROFILE_SCOPE();

	VoxelDebug::DebugRenderer &dr = _debug_renderer;
	dr.begin();

	const Transform parent_transform = get_global_transform();

	const int octree_size = get_block_size() << (get_lod_count() - 1);
	const Basis local_octree_basis = Basis().scaled(Vector3(octree_size, octree_size, octree_size));
	for (Map<Vector3i, OctreeItem>::Element *E = _lod_octrees.front(); E; E = E->next()) {
		const Transform local_transform(local_octree_basis, (E->key() * octree_size).to_vec3());
		dr.draw_box(parent_transform * local_transform, VoxelDebug::ID_OCTREE_BOUNDS);
	}

	const float bounds_in_voxels_len = _bounds_in_voxels.size.length();
	if (bounds_in_voxels_len < 10000) {
		const Vector3 margin = Vector3(1, 1, 1) * bounds_in_voxels_len * 0.0025f;
		const Vector3 size = _bounds_in_voxels.size.to_vec3();
		const Transform local_transform(Basis().scaled(size + margin * 2.f), _bounds_in_voxels.pos.to_vec3() - margin);
		dr.draw_box(parent_transform * local_transform, VoxelDebug::ID_VOXEL_BOUNDS);
	}

	dr.end();
}

void VoxelLodTerrain::set_show_gizmos(bool enable) {
	_show_gizmos_enabled = enable;
	if (_show_gizmos_enabled) {
		_debug_renderer.set_world(is_visible_in_tree() ? *get_world() : nullptr);
	} else {
		_debug_renderer.clear();
	}
}

#endif

Array VoxelLodTerrain::_b_debug_print_sdf_top_down(Vector3 center, Vector3 extents) const {
	Array image_array;
	image_array.resize(get_lod_count());

	for (int lod_index = 0; lod_index < get_lod_count(); ++lod_index) {
		const Rect3i world_box = Rect3i::from_center_extents(Vector3i(center) >> lod_index, Vector3i(extents) >> lod_index);

		if (world_box.size.volume() == 0) {
			continue;
		}

		Ref<VoxelBuffer> buffer_ref;
		buffer_ref.instance();
		VoxelBuffer &buffer = **buffer_ref;
		buffer.create(world_box.size);

		const Lod &lod = _lods[lod_index];

		world_box.for_each_cell([&](const Vector3i &world_pos) {
			const float v = lod.map.get_voxel_f(world_pos, VoxelBuffer::CHANNEL_SDF);
			const Vector3i rpos = world_pos - world_box.pos;
			buffer.set_voxel_f(v, rpos.x, rpos.y, rpos.z, VoxelBuffer::CHANNEL_SDF);
		});

		Ref<Image> image = buffer.debug_print_sdf_to_image_top_down();
		image_array[lod_index] = image;
	}

	return image_array;
}

int VoxelLodTerrain::_b_debug_get_block_count() const {
	int sum = 0;
	for (int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		sum += _lods[lod_index].map.get_block_count();
	}
	return sum;
}

Error VoxelLodTerrain::_b_debug_dump_as_scene(String fpath) const {
	Spatial *root = memnew(Spatial);
	root->set_name(get_name());

	for (int lod_index = 0; lod_index < _lod_count; ++lod_index) {
		const Lod &lod = _lods[lod_index];

		lod.map.for_all_blocks([root](const VoxelBlock *block) {
			block->for_each_mesh_instance_with_transform([root, block](const DirectMeshInstance &dmi, Transform t) {
				Ref<Mesh> mesh = dmi.get_mesh();

				if (mesh.is_valid()) {
					MeshInstance *mi = memnew(MeshInstance);
					mi->set_mesh(mesh);
					mi->set_transform(t);
					// TODO Transition mesh visibility?
					mi->set_visible(block->is_visible());
					root->add_child(mi);
					// The owner must be set after adding to parent
					mi->set_owner(root);
				}
			});
		});
	}

	Ref<PackedScene> scene = memnew(PackedScene);
	Error err = scene->pack(root);
	if (err != OK) {
		return err;
	}
	err = ResourceSaver::save(fpath, scene, ResourceSaver::FLAG_BUNDLE_RESOURCES);

	memdelete(root);

	return err;
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

	ClassDB::bind_method(D_METHOD("set_lod_count", "lod_count"), &VoxelLodTerrain::set_lod_count);
	ClassDB::bind_method(D_METHOD("get_lod_count"), &VoxelLodTerrain::get_lod_count);

	ClassDB::bind_method(D_METHOD("set_lod_split_scale", "lod_split_scale"), &VoxelLodTerrain::set_lod_split_scale);
	ClassDB::bind_method(D_METHOD("get_lod_split_scale"), &VoxelLodTerrain::get_lod_split_scale);

	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelLodTerrain::get_block_size);
	ClassDB::bind_method(D_METHOD("get_block_region_extent"), &VoxelLodTerrain::get_block_region_extent);
	ClassDB::bind_method(D_METHOD("get_statistics"), &VoxelLodTerrain::get_statistics);
	ClassDB::bind_method(D_METHOD("voxel_to_block_position", "lod_index"), &VoxelLodTerrain::voxel_to_block_position);

	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelLodTerrain::get_voxel_tool);
	ClassDB::bind_method(D_METHOD("save_modified_blocks"), &VoxelLodTerrain::_b_save_modified_blocks);

	ClassDB::bind_method(D_METHOD("set_run_stream_in_editor"), &VoxelLodTerrain::set_run_stream_in_editor);
	ClassDB::bind_method(D_METHOD("is_stream_running_in_editor"), &VoxelLodTerrain::is_stream_running_in_editor);

	ClassDB::bind_method(D_METHOD("set_voxel_bounds"), &VoxelLodTerrain::_b_set_voxel_bounds);
	ClassDB::bind_method(D_METHOD("get_voxel_bounds"), &VoxelLodTerrain::_b_get_voxel_bounds);

	ClassDB::bind_method(D_METHOD("set_process_mode", "mode"), &VoxelLodTerrain::set_process_mode);
	ClassDB::bind_method(D_METHOD("get_process_mode"), &VoxelLodTerrain::get_process_mode);

	ClassDB::bind_method(D_METHOD("debug_raycast_block", "origin", "dir"), &VoxelLodTerrain::debug_raycast_block);
	ClassDB::bind_method(D_METHOD("debug_get_block_info", "block_pos", "lod"), &VoxelLodTerrain::debug_get_block_info);
	ClassDB::bind_method(D_METHOD("debug_get_octrees"), &VoxelLodTerrain::debug_get_octrees);
	ClassDB::bind_method(D_METHOD("debug_print_sdf_top_down", "center", "extents"),
			&VoxelLodTerrain::_b_debug_print_sdf_top_down);
	ClassDB::bind_method(D_METHOD("debug_get_block_count"), &VoxelLodTerrain::_b_debug_get_block_count);
	ClassDB::bind_method(D_METHOD("debug_dump_as_scene", "path"), &VoxelLodTerrain::_b_debug_dump_as_scene);

	//ClassDB::bind_method(D_METHOD("_on_stream_params_changed"), &VoxelLodTerrain::_on_stream_params_changed);

	BIND_ENUM_CONSTANT(PROCESS_MODE_IDLE);
	BIND_ENUM_CONSTANT(PROCESS_MODE_PHYSICS);
	BIND_ENUM_CONSTANT(PROCESS_MODE_DISABLED);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"),
			"set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "view_distance"), "set_view_distance", "get_view_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count"), "set_lod_count", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lod_split_scale"), "set_lod_split_scale", "get_lod_split_scale");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
			"set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "generate_collisions"),
			"set_generate_collisions", "get_generate_collisions");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_lod_count"),
			"set_collision_lod_count", "get_collision_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "run_stream_in_editor"),
			"set_run_stream_in_editor", "is_stream_running_in_editor");
	ADD_PROPERTY(PropertyInfo(Variant::AABB, "voxel_bounds"), "set_voxel_bounds", "get_voxel_bounds");
}
