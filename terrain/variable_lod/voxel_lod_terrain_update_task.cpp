#include "voxel_lod_terrain_update_task.h"
#include "../../engine/generate_block_task.h"
#include "../../engine/load_block_data_task.h"
#include "../../engine/mesh_block_task.h"
#include "../../engine/save_block_data_task.h"
#include "../../engine/voxel_engine.h"
#include "../../meshers/transvoxel/voxel_mesher_transvoxel.h"
#include "../../util/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/godot/classes/engine.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"
#include "../../util/tasks/async_dependency_tracker.h"

namespace zylann::voxel {

void VoxelLodTerrainUpdateTask::flush_pending_lod_edits(
		VoxelLodTerrainUpdateData::State &state, VoxelData &data, const int mesh_block_size) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	static thread_local std::vector<Vector3i> tls_modified_lod0_blocks;
	static thread_local std::vector<VoxelData::BlockLocation> tls_updated_block_locations;

	const int data_block_size = data.get_block_size();
	const int data_to_mesh_factor = mesh_block_size / data_block_size;

	{
		MutexLock lock(state.blocks_pending_lodding_lod0_mutex);
		// Not sure if could just use `=`? What would std::vector do with capacity?
		tls_modified_lod0_blocks.resize(state.blocks_pending_lodding_lod0.size());
		memcpy(tls_modified_lod0_blocks.data(), state.blocks_pending_lodding_lod0.data(),
				state.blocks_pending_lodding_lod0.size() * sizeof(Vector3i));

		state.blocks_pending_lodding_lod0.clear();
	}

	tls_updated_block_locations.clear();
	data.update_lods(to_span(tls_modified_lod0_blocks), &tls_updated_block_locations);

	// Schedule mesh updates at every affected LOD
	for (const VoxelData::BlockLocation loc : tls_updated_block_locations) {
		const Vector3i mesh_block_pos = math::floordiv(loc.position, data_to_mesh_factor);
		VoxelLodTerrainUpdateData::Lod &dst_lod = state.lods[loc.lod_index];

		auto mesh_block_it = dst_lod.mesh_map_state.map.find(mesh_block_pos);
		if (mesh_block_it != dst_lod.mesh_map_state.map.end()) {
			// If a mesh exists here, it will need an update.
			// If there is no mesh, it will probably get created later when we come closer to it
			schedule_mesh_update(mesh_block_it->second, mesh_block_pos, dst_lod.blocks_pending_update);
		}
	}
}

static void process_unload_data_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, VoxelData &data,
		Vector3 p_viewer_pos, std::vector<VoxelData::BlockToSave> &blocks_to_save, bool can_save,
		const VoxelLodTerrainUpdateData::Settings &settings) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box data unload");
	// TODO Could it actually be enough to have a rolling update on all blocks?

	// This should be the same distance relatively to each LOD
	const int data_block_size = data.get_block_size();
	const int data_block_size_po2 = data.get_block_size_po2();
	const int data_block_region_extent =
			VoxelEngine::get_octree_lod_block_region_extent(settings.lod_distance, data_block_size);
	const Box3i bounds_in_voxels = data.get_bounds();

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;

	const int lod_count = data.get_lod_count();

	static thread_local std::vector<Box3i> tls_to_remove;
	tls_to_remove.clear();

	// Ignore largest lod because it can extend a little beyond due to the view distance setting.
	// Instead, those blocks are unloaded by the octree forest management.
	// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
	for (int lod_index = lod_count - 2; lod_index >= 0; --lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
		// The player can edit them so changes can be propagated to lower lods.

		const unsigned int block_size_po2 = data_block_size_po2 + lod_index;
		const Vector3i viewer_block_pos_within_lod =
				VoxelDataMap::voxel_to_block_b(math::floor_to_int(p_viewer_pos), block_size_po2);

		const Box3i bounds_in_blocks = Box3i( //
				bounds_in_voxels.pos >> block_size_po2, //
				bounds_in_voxels.size >> block_size_po2);

		const Box3i new_box =
				Box3i::from_center_extents(viewer_block_pos_within_lod, Vector3iUtil::create(data_block_region_extent));
		const Box3i prev_box = Box3i::from_center_extents(
				lod.last_viewer_data_block_pos, Vector3iUtil::create(lod.last_view_distance_data_blocks));

		if (!new_box.intersects(bounds_in_blocks) && !prev_box.intersects(bounds_in_blocks)) {
			// If this box doesn't intersect either now or before, there is no chance a smaller one will
			break;
		}

		// Eliminate pending blocks that aren't needed

		if (prev_box != new_box) {
			ZN_PROFILE_SCOPE_NAMED("Unload data");
			// VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			// RWLockWrite wlock(data_lod.map_lock);

			tls_to_remove.clear();
			prev_box.difference_to_vec(new_box, tls_to_remove);

			for (const Box3i bbox : tls_to_remove) {
				data.unload_blocks(bbox, lod_index, &blocks_to_save);
			}
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Cancel updates");
			// Cancel block updates that are not within the padded region
			// (since neighbors are always required to remesh)

			const Box3i padded_new_box = new_box.padded(-1);
			Box3i mesh_box;
			if (mesh_block_size > data_block_size) {
				const int factor = mesh_block_size / data_block_size;
				mesh_box = padded_new_box.downscaled_inner(factor);
			} else {
				mesh_box = padded_new_box;
			}

			unordered_remove_if(lod.blocks_pending_update, [&lod, mesh_box](Vector3i bpos) {
				if (mesh_box.contains(bpos)) {
					return false;
				} else {
					auto mesh_block_it = lod.mesh_map_state.map.find(bpos);
					if (mesh_block_it != lod.mesh_map_state.map.end()) {
						mesh_block_it->second.state = VoxelLodTerrainUpdateData::MESH_NEED_UPDATE;
					}
					return true;
				}
			});
		}

		lod.last_viewer_data_block_pos = viewer_block_pos_within_lod;
		lod.last_view_distance_data_blocks = data_block_region_extent;
	}
}

static void process_unload_mesh_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, Vector3 p_viewer_pos,
		const VoxelLodTerrainUpdateData::Settings &settings, const VoxelData &data) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box mesh unload");
	// TODO Could it actually be enough to have a rolling update on all blocks?

	// This should be the same distance relatively to each LOD
	const int mesh_block_size_po2 = settings.mesh_block_size_po2;
	const int mesh_block_size = 1 << mesh_block_size_po2;
	const int mesh_block_region_extent =
			VoxelEngine::get_octree_lod_block_region_extent(settings.lod_distance, mesh_block_size);
	const int lod_count = data.get_lod_count();
	const Box3i bounds_in_voxels = data.get_bounds();

	// Ignore largest lod because it can extend a little beyond due to the view distance setting.
	// Instead, those blocks are unloaded by the octree forest management.
	// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
	for (int lod_index = lod_count - 2; lod_index >= 0; --lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		unsigned int block_size_po2 = mesh_block_size_po2 + lod_index;
		const Vector3i viewer_block_pos_within_lod = math::floor_to_int(p_viewer_pos) >> block_size_po2;

		const Box3i bounds_in_blocks = Box3i( //
				bounds_in_voxels.pos >> block_size_po2, //
				bounds_in_voxels.size >> block_size_po2);

		const Box3i new_box =
				Box3i::from_center_extents(viewer_block_pos_within_lod, Vector3iUtil::create(mesh_block_region_extent));
		const Box3i prev_box = Box3i::from_center_extents(
				lod.last_viewer_mesh_block_pos, Vector3iUtil::create(lod.last_view_distance_mesh_blocks));

		if (!new_box.intersects(bounds_in_blocks) && !prev_box.intersects(bounds_in_blocks)) {
			// If this box doesn't intersect either now or before, there is no chance a smaller one will
			break;
		}

		// Eliminate pending blocks that aren't needed

		if (prev_box != new_box) {
			ZN_PROFILE_SCOPE_NAMED("Unload meshes");
			RWLockWrite wlock(lod.mesh_map_state.map_lock);
			prev_box.difference(new_box, [&lod](Box3i out_of_range_box) {
				out_of_range_box.for_each_cell([&lod](Vector3i pos) {
					// print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
					// unload_mesh_block(pos, lod_index);
					lod.mesh_map_state.map.erase(pos);
					lod.mesh_blocks_to_unload.push_back(pos);
				});
			});
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Cancel updates");
			// Cancel block updates that are not within the new region
			unordered_remove_if(lod.blocks_pending_update, [new_box](Vector3i bpos) { //
				return !new_box.contains(bpos);
			});
		}

		lod.last_viewer_mesh_block_pos = viewer_block_pos_within_lod;
		lod.last_view_distance_mesh_blocks = mesh_block_region_extent;
	}
}

void process_octrees_sliding_box(VoxelLodTerrainUpdateData::State &state, Vector3 p_viewer_pos,
		const VoxelLodTerrainUpdateData::Settings &settings, const VoxelData &data) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box octrees");
	// TODO Investigate if multi-octree can produce cracks in the terrain (so far I haven't noticed)

	const unsigned int lod_count = data.get_lod_count();
	const unsigned int mesh_block_size_po2 = settings.mesh_block_size_po2;
	const unsigned int octree_size_po2 = LodOctree::get_octree_size_po2(mesh_block_size_po2, lod_count);
	const unsigned int octree_size = 1 << octree_size_po2;
	const unsigned int octree_region_extent = 1 + settings.view_distance_voxels / (1 << octree_size_po2);

	const Vector3i viewer_octree_pos =
			(math::floor_to_int(p_viewer_pos) + Vector3iUtil::create(octree_size / 2)) >> octree_size_po2;

	const Box3i bounds_in_octrees = data.get_bounds().downscaled(octree_size);

	const Box3i new_box = Box3i::from_center_extents(viewer_octree_pos, Vector3iUtil::create(octree_region_extent))
								  .clipped(bounds_in_octrees);
	const Box3i prev_box = state.last_octree_region_box;

	if (new_box != prev_box) {
		struct CleanOctreeAction {
			VoxelLodTerrainUpdateData::State &state;
			Vector3i block_offset_lod0;

			void operator()(Vector3i node_pos, unsigned int lod_index) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

				Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);

				auto block_it = lod.mesh_map_state.map.find(bpos);
				if (block_it != lod.mesh_map_state.map.end()) {
					lod.mesh_blocks_to_deactivate.push_back(bpos);
					block_it->second.active = false;
				}
			}
		};

		struct ExitAction {
			VoxelLodTerrainUpdateData::State &state;
			unsigned int lod_count;

			void operator()(const Vector3i &pos) {
				std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::iterator it = state.lod_octrees.find(pos);
				if (it == state.lod_octrees.end()) {
					return;
				}

				VoxelLodTerrainUpdateData::OctreeItem &item = it->second;
				const Vector3i block_pos_maxlod = it->first;

				const unsigned int last_lod_index = lod_count - 1;

				// We just drop the octree and hide blocks it was considering as visible.
				// Normally such octrees shouldn't bee too deep as they will likely be at the edge
				// of the loaded area, unless the player teleported far away.
				CleanOctreeAction a{ state, block_pos_maxlod << last_lod_index };
				item.octree.clear(a);

				state.lod_octrees.erase(it);

				// Unload last lod from here, as it may extend a bit further than the others.
				// Other LODs are unloaded earlier using a sliding region.
				VoxelLodTerrainUpdateData::Lod &last_lod = state.lods[last_lod_index];
				last_lod.mesh_map_state.map.erase(pos);
				last_lod.mesh_blocks_to_unload.push_back(pos);
			}
		};

		struct EnterAction {
			VoxelLodTerrainUpdateData::State &state;
			unsigned int lod_count;

			void operator()(const Vector3i &pos) {
				// That's a new cell we are entering, shouldn't be anything there
				CRASH_COND(state.lod_octrees.find(pos) != state.lod_octrees.end());

				// Create new octree
				// TODO Use ObjectPool to store them, deletion won't be cheap
				std::pair<std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::iterator, bool> p =
						state.lod_octrees.insert({ pos, VoxelLodTerrainUpdateData::OctreeItem() });
				CRASH_COND(p.second == false);
				VoxelLodTerrainUpdateData::OctreeItem &item = p.first->second;
				LodOctree::NoDestroyAction nda;
				item.octree.create(lod_count, nda);
			}
		};

		ExitAction exit_action{ state, lod_count };
		EnterAction enter_action{ state, lod_count };
		{
			ZN_PROFILE_SCOPE_NAMED("Unload octrees");

			const unsigned int last_lod_index = lod_count - 1;
			VoxelLodTerrainUpdateData::Lod &last_lod = state.lods[last_lod_index];
			RWLockWrite wlock(last_lod.mesh_map_state.map_lock);

			prev_box.difference(new_box, [exit_action](Box3i out_of_range_box) { //
				out_of_range_box.for_each_cell(exit_action);
			});
		}
		{
			ZN_PROFILE_SCOPE_NAMED("Load octrees");
			new_box.difference(prev_box, [enter_action](Box3i box_to_load) { //
				box_to_load.for_each_cell(enter_action);
			});
		}

		state.force_update_octrees_next_update = true;
	}

	state.last_octree_region_box = new_box;
}

inline bool check_block_sizes(int data_block_size, int mesh_block_size) {
	return (data_block_size == 16 || data_block_size == 32) && (mesh_block_size == 16 || mesh_block_size == 32) &&
			mesh_block_size >= data_block_size;
}

bool check_block_mesh_updated(VoxelLodTerrainUpdateData::State &state, const VoxelData &data,
		VoxelLodTerrainUpdateData::MeshBlockState &mesh_block, Vector3i mesh_block_pos, uint8_t lod_index,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings) {
	// ZN_PROFILE_SCOPE();

	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	const VoxelLodTerrainUpdateData::MeshState mesh_state = mesh_block.state;

	switch (mesh_state) {
		case VoxelLodTerrainUpdateData::MESH_NEVER_UPDATED:
		case VoxelLodTerrainUpdateData::MESH_NEED_UPDATE: {
			bool surrounded = true;
			if (data.is_streaming_enabled()) {
				const int mesh_block_size = 1 << settings.mesh_block_size_po2;
				const int data_block_size = data.get_block_size();
#ifdef DEBUG_ENABLED
				ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif
				// TODO Why are we only checking neighbors?
				// This is also redundant when called from `check_block_loaded_and_meshed`

				// Find data block neighbors positions
				const int factor = mesh_block_size / data_block_size;
				const Vector3i data_block_pos0 = factor * mesh_block_pos;
				const Box3i data_box(
						data_block_pos0 - Vector3i(1, 1, 1), Vector3iUtil::create(factor) + Vector3i(2, 2, 2));
				const Box3i bounds = data.get_bounds().downscaled(data_block_size);
				// 56 is the maximum amount of positions that can be gathered this way with mesh block size 32.
				FixedArray<Vector3i, 56> neighbor_positions;
				unsigned int neighbor_positions_count = 0;
				data_box.for_inner_outline([bounds, &neighbor_positions, &neighbor_positions_count](Vector3i pos) {
					if (bounds.contains(pos)) {
						neighbor_positions[neighbor_positions_count] = pos;
						++neighbor_positions_count;
					}
				});

				static thread_local std::vector<Vector3i> tls_missing;
				tls_missing.clear();

				// Check if neighbors are loaded
				data.get_missing_blocks(to_span(neighbor_positions, neighbor_positions_count), lod_index, tls_missing);

				surrounded = tls_missing.size() == 0;

				// Schedule loading for missing neighbors
				MutexLock lock(lod.loading_blocks_mutex);
				for (const Vector3i &missing_pos : tls_missing) {
					if (!lod.has_loading_block(missing_pos)) {
						blocks_to_load.push_back({ missing_pos, lod_index });
						lod.loading_blocks.insert(missing_pos);
					}
				}
			}

			if (surrounded) {
				lod.blocks_pending_update.push_back(mesh_block_pos);
				mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
			}

			return false;
		}

		case VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT:
		case VoxelLodTerrainUpdateData::MESH_UPDATE_SENT:
			return false;

		case VoxelLodTerrainUpdateData::MESH_UP_TO_DATE:
			return true;

		default:
			CRASH_NOW();
			break;
	}

	return true;
}

VoxelLodTerrainUpdateData::MeshBlockState &insert_new(
		std::unordered_map<Vector3i, VoxelLodTerrainUpdateData::MeshBlockState> &mesh_map, Vector3i pos) {
#ifdef DEBUG_ENABLED
	// We got here because the map didn't contain the element. If it did contain it already, that's a bug.
	static VoxelLodTerrainUpdateData::MeshBlockState s_default;
	ERR_FAIL_COND_V(mesh_map.find(pos) != mesh_map.end(), s_default);
#endif
	// C++ standard says if the element is not present, it will be default-constructed.
	// So here is how to insert a default, non-movable struct into an unordered_map.
	// https://stackoverflow.com/questions/22229773/map-unordered-map-with-non-movable-default-constructible-value-type
	VoxelLodTerrainUpdateData::MeshBlockState &block = mesh_map[pos];

	// This approach doesn't compile, had to workaround with the writing [] operator.
	/*
	auto p = lod.mesh_map_state.map.emplace(pos, VoxelLodTerrainUpdateData::MeshBlockState());
	// We got here because the map didn't contain the element. If it did contain it already, that's a bug.
	CRASH_COND(p.second == false);
	*/

	return block;
}

static bool check_block_loaded_and_meshed(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const VoxelData &data, const Vector3i &p_mesh_block_pos,
		uint8_t lod_index, std::vector<VoxelLodTerrainUpdateData::BlockLocation> &blocks_to_load) {
	//

	if (data.is_streaming_enabled()) {
		const int mesh_block_size = 1 << settings.mesh_block_size_po2;
		const int data_block_size = data.get_block_size();

#ifdef DEBUG_ENABLED
		ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif
		// We want to know everything about the data intersecting this mesh block.
		// This is not known in advance when we stream it, it has to be requested.
		// When not streaming, `block == null` is the same as `!block->has_voxels()` so we wouldn't need to enter here.

		static thread_local std::vector<Vector3i> tls_missing;
		tls_missing.clear();

		const int factor = mesh_block_size / data_block_size;
		const Box3i data_blocks_box = Box3i(p_mesh_block_pos * factor, Vector3iUtil::create(factor)).padded(1);

		data.get_missing_blocks(data_blocks_box, lod_index, tls_missing);

		if (tls_missing.size() > 0) {
			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			MutexLock mlock(lod.loading_blocks_mutex);
			for (const Vector3i &missing_bpos : tls_missing) {
				if (!lod.has_loading_block(missing_bpos)) {
					blocks_to_load.push_back({ missing_bpos, lod_index });
					lod.loading_blocks.insert(missing_bpos);
				}
			}
			return false;
		}
	}

	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	VoxelLodTerrainUpdateData::MeshBlockState *mesh_block = nullptr;
	auto mesh_block_it = lod.mesh_map_state.map.find(p_mesh_block_pos);
	if (mesh_block_it == lod.mesh_map_state.map.end()) {
		// If this ever becomes a source of contention with the main thread's `apply_mesh_update`,
		// we could defer additions to the end of octree fitting.
		RWLockWrite wlock(lod.mesh_map_state.map_lock);
		mesh_block = &insert_new(lod.mesh_map_state.map, p_mesh_block_pos);
	} else {
		mesh_block = &mesh_block_it->second;
	}

	return check_block_mesh_updated(state, data, *mesh_block, p_mesh_block_pos, lod_index, blocks_to_load, settings);
}

uint8_t VoxelLodTerrainUpdateTask::get_transition_mask(const VoxelLodTerrainUpdateData::State &state,
		Vector3i block_pos, unsigned int lod_index, unsigned int lod_count) {
	uint8_t transition_mask = 0;

	if (lod_index + 1 >= lod_count) {
		// We do transitions on higher-resolution blocks.
		// Therefore, lowest-resolution blocks never have transitions.
		return transition_mask;
	}

	const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	// Based on octree rules, and the fact it must have run before, check neighbor blocks of same LOD:
	// If one is missing or not visible, it means either of the following:
	// - The neighbor at lod+1 is visible or not loaded (there must be a transition)
	// - The neighbor at lod-1 is visible (no transition)

	uint8_t visible_neighbors_of_same_lod = 0;
	for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		const Vector3i npos = block_pos + Cube::g_side_normals[dir];

		auto nblock_it = lod.mesh_map_state.map.find(npos);

		if (nblock_it != lod.mesh_map_state.map.end() && nblock_it->second.active) {
			visible_neighbors_of_same_lod |= (1 << dir);
		}
	}

	if (visible_neighbors_of_same_lod == 0b111111) {
		// No transitions needed
		return transition_mask;
	}

	{
		const Vector3i lower_pos = block_pos >> 1;
		const Vector3i upper_pos = block_pos << 1;

		const VoxelLodTerrainUpdateData::Lod &lower_lod = state.lods[lod_index + 1];

		// At least one neighbor isn't visible.
		// Check for neighbors at different LOD (there can be only one kind on a given side)
		for (unsigned int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
			const unsigned int dir_mask = (1 << dir);

			if ((visible_neighbors_of_same_lod & dir_mask) != 0) {
				continue;
			}

			const Vector3i side_normal = Cube::g_side_normals[dir];
			const Vector3i lower_neighbor_pos = (block_pos + side_normal) >> 1;

			if (lower_neighbor_pos != lower_pos) {
				auto lower_neighbor_block_it = lower_lod.mesh_map_state.map.find(lower_neighbor_pos);

				if (lower_neighbor_block_it != lower_lod.mesh_map_state.map.end() &&
						lower_neighbor_block_it->second.active) {
					// The block has a visible neighbor of lower LOD
					transition_mask |= dir_mask;
					continue;
				}
			}

			if (lod_index > 0) {
				// Check upper LOD neighbors.
				// There are always 4 on each side, checking any is enough

				Vector3i upper_neighbor_pos = upper_pos;
				for (unsigned int i = 0; i < Vector3iUtil::AXIS_COUNT; ++i) {
					if (side_normal[i] == -1) {
						--upper_neighbor_pos[i];
					} else if (side_normal[i] == 1) {
						upper_neighbor_pos[i] += 2;
					}
				}

				const VoxelLodTerrainUpdateData::Lod &upper_lod = state.lods[lod_index - 1];
				auto upper_neighbor_block_it = upper_lod.mesh_map_state.map.find(upper_neighbor_pos);

				if (upper_neighbor_block_it == upper_lod.mesh_map_state.map.end() ||
						upper_neighbor_block_it->second.active == false) {
					// The block has no visible neighbor yet. World border? Assume lower LOD.
					transition_mask |= dir_mask;
				}
			}
		}
	}

	return transition_mask;
}

static void process_octrees_fitting(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, VoxelData &data, Vector3 p_viewer_pos,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load) {
	//
	ZN_PROFILE_SCOPE();

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int octree_leaf_node_size = mesh_block_size;
	const unsigned int lod_count = data.get_lod_count();

	const bool force_update_octrees = state.force_update_octrees_next_update;
	state.force_update_octrees_next_update = false;

	// Octrees may not need to update every frame under certain conditions
	if (!state.had_blocked_octree_nodes_previous_update && !force_update_octrees &&
			p_viewer_pos.distance_squared_to(Vector3(state.local_viewer_pos_previous_octree_update)) <
					math::squared(octree_leaf_node_size / 2)) {
		return;
	}

	state.local_viewer_pos_previous_octree_update = p_viewer_pos;

	const float lod_distance_octree_space = settings.lod_distance / octree_leaf_node_size;

	unsigned int blocked_octree_nodes = 0;

	// Off by one bit: second bit is LOD0, first bit is unused
	uint32_t lods_to_update_transitions = 0;

	// TODO Optimization: Maintain a vector to make iteration faster?
	for (auto octree_it = state.lod_octrees.begin(); octree_it != state.lod_octrees.end(); ++octree_it) {
		ZN_PROFILE_SCOPE();

		struct OctreeActions {
			VoxelLodTerrainUpdateData::State &state;
			const VoxelLodTerrainUpdateData::Settings &settings;
			VoxelData &data;
			std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load;
			Vector3i block_offset_lod0;
			unsigned int blocked_count = 0;
			float lod_distance_octree_space;
			Vector3 viewer_pos_octree_space;
			uint32_t &lods_to_update_transitions;

			void create_child(Vector3i node_pos, int lod_index, LodOctree::NodeData &node_data) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
				const Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				// Never show a child that hasn't been meshed, if we got here that would be a bug
				CRASH_COND(mesh_block_it == lod.mesh_map_state.map.end());
				CRASH_COND(mesh_block_it->second.state != VoxelLodTerrainUpdateData::MESH_UP_TO_DATE);

				// self->set_mesh_block_active(*block, true);
				lod.mesh_blocks_to_activate.push_back(bpos);
				mesh_block_it->second.active = true;
				lods_to_update_transitions |= (0b111 << lod_index);
			}

			void destroy_child(Vector3i node_pos, int lod_index) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
				const Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				if (mesh_block_it != lod.mesh_map_state.map.end()) {
					// self->set_mesh_block_active(*block, false);
					mesh_block_it->second.active = false;
					lod.mesh_blocks_to_deactivate.push_back(bpos);
					lods_to_update_transitions |= (0b111 << lod_index);
				}
			}

			void show_parent(Vector3i node_pos, int lod_index) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
				Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				// If we teleport far away, the area we were in is going to merge,
				// and blocks may have been unloaded completely.
				// So in that case it's normal to not find any block.
				// Otherwise, there must always be a visible parent in the end, unless the octree vanished.
				if (mesh_block_it != lod.mesh_map_state.map.end() &&
						mesh_block_it->second.state == VoxelLodTerrainUpdateData::MESH_UP_TO_DATE) {
					// self->set_mesh_block_active(*block, true);
					mesh_block_it->second.active = true;
					lod.mesh_blocks_to_activate.push_back(bpos);
					lods_to_update_transitions |= (0b111 << lod_index);
				}
			}

			void hide_parent(Vector3i node_pos, int lod_index) {
				destroy_child(node_pos, lod_index); // Same
			}

			bool can_create_root(int lod_index) {
				const Vector3i offset = block_offset_lod0 >> lod_index;
				const bool can =
						check_block_loaded_and_meshed(state, settings, data, offset, lod_index, data_blocks_to_load);
				if (!can) {
					++blocked_count;
				}
				return can;
			}

			bool can_split(Vector3i node_pos, int lod_index, LodOctree::NodeData &node_data) {
				ZN_PROFILE_SCOPE();
				if (!LodOctree::is_below_split_distance(
							node_pos, lod_index, viewer_pos_octree_space, lod_distance_octree_space)) {
					return false;
				}
				const int child_lod_index = lod_index - 1;
				const Vector3i offset = block_offset_lod0 >> child_lod_index;
				bool can = true;

				// Can only subdivide if higher detail meshes are ready to be shown, otherwise it will produce holes
				for (int i = 0; i < 8; ++i) {
					// Get block pos local-to-region + convert to local-to-terrain
					const Vector3i child_pos = LodOctree::get_child_position(node_pos, i) + offset;
					// We have to ping ALL children, because the reason we are here is we want them loaded
					can &= check_block_loaded_and_meshed(
							state, settings, data, child_pos, child_lod_index, data_blocks_to_load);
				}

				// Can only subdivide if blocks of a higher LOD index are present around,
				// otherwise it will cause cracks.
				// Need to check meshes, not voxels?
				// const int lod_index = child_lod_index + 1;
				// if (lod_index < self->get_lod_count()) {
				// 	const Vector3i parent_offset = block_offset_lod0 >> lod_index;
				// 	const Lod &lod = self->_lods[lod_index];
				// 	can &= self->is_block_surrounded(node_pos + parent_offset, lod_index, lod.map);
				// }

				if (!can) {
					++blocked_count;
				}

				return can;
			}

			bool can_join(Vector3i node_pos, int parent_lod_index) {
				ZN_PROFILE_SCOPE();
				if (LodOctree::is_below_split_distance(
							node_pos, parent_lod_index, viewer_pos_octree_space, lod_distance_octree_space)) {
					return false;
				}
				// Can only unsubdivide if the parent mesh is ready
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[parent_lod_index];

				Vector3i bpos = node_pos + (block_offset_lod0 >> parent_lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				if (mesh_block_it == lod.mesh_map_state.map.end()) {
					// The block got unloaded. Exceptionally, we can join.
					// There will always be a grand-parent because we never destroy them when they split,
					// and we never create a child without creating a parent first.
					return true;
				}

				// The block is loaded (?) but the mesh isn't up to date, we need to ping and wait.
				const bool can = check_block_mesh_updated(
						state, data, mesh_block_it->second, bpos, parent_lod_index, data_blocks_to_load, settings);

				if (!can) {
					++blocked_count;
				}

				return can;
			}
		};

		const Vector3i block_pos_maxlod = octree_it->first;
		const Vector3i block_offset_lod0 = block_pos_maxlod << (lod_count - 1);
		const Vector3 relative_viewer_pos = p_viewer_pos - Vector3(mesh_block_size * block_offset_lod0);

		OctreeActions octree_actions{ //
			state, //
			settings, //
			data, //
			data_blocks_to_load, //
			block_offset_lod0, //
			0, //
			lod_distance_octree_space, //
			relative_viewer_pos / octree_leaf_node_size, //
			lods_to_update_transitions
		};
		VoxelLodTerrainUpdateData::OctreeItem &item = octree_it->second;
		item.octree.update(octree_actions);

		blocked_octree_nodes += octree_actions.blocked_count;
	}

	// Ideally, this stat should stabilize to zero.
	// If not, something in block management prevents LODs from properly show up and should be fixed.
	state.stats.blocked_lods = blocked_octree_nodes;
	state.had_blocked_octree_nodes_previous_update = blocked_octree_nodes > 0;

	// TODO Optimize: this works but it's not smart.
	// It doesn't take too long (100 microseconds when octrees update with lod distance 60).
	// We used to only update positions based on which blocks were added/removed in the octree update,
	// which was faster than this. However it missed some spots, which caused annoying cracks to show up.
	// So instead, when any block changes state in LOD N, we update all transitions in LODs N-1, N, and N+1.
	// It is unclear yet why the old approach didn't work, maybe because it didn't properly made N-1 and N+1 update.
	// If you find a better approach, it has to comply with the validation check below.
	if (lods_to_update_transitions != 0) {
		ZN_PROFILE_SCOPE_NAMED("Transition masks");
		lods_to_update_transitions >>= 1;
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			if ((lods_to_update_transitions & (1 << lod_index)) == 0) {
				continue;
			}
			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			RWLockRead rlock(lod.mesh_map_state.map_lock);
			for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
				if (it->second.active) {
					const uint8_t recomputed_mask =
							VoxelLodTerrainUpdateTask::get_transition_mask(state, it->first, lod_index, lod_count);
					if (recomputed_mask != it->second.transition_mask) {
						it->second.transition_mask = recomputed_mask;
						lod.mesh_blocks_to_update_transitions.push_back(
								VoxelLodTerrainUpdateData::TransitionUpdate{ it->first, recomputed_mask });
					}
				}
			}
		}
	}
#if 0
	// DEBUG: Validation check for transition mask updates.
	{
		ZN_PROFILE_SCOPE_NAMED("Transition checks");
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			RWLockRead rlock(lod.mesh_map_state.map_lock);
			for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
				if (it->second.active) {
					const uint8_t recomputed_mask =
							VoxelLodTerrainUpdateTask::get_transition_mask(state, it->first, lod_index, lod_count);
					CRASH_COND(recomputed_mask != it->second.transition_mask);
				}
			}
		}
	}
#endif
}

inline Vector3i get_block_center(Vector3i pos, int bs, int lod) {
	return (pos << lod) * bs + Vector3iUtil::create(bs / 2);
}

static void init_sparse_octree_priority_dependency(PriorityDependency &dep, Vector3i block_position, uint8_t lod,
		int data_block_size, std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data,
		const Transform3D &volume_transform, float octree_lod_distance) {
	//
	const Vector3i voxel_pos = get_block_center(block_position, data_block_size, lod);
	const float block_radius = (data_block_size << lod) / 2;
	dep.shared = shared_viewers_data;
	dep.world_position = volume_transform.xform(voxel_pos);
	const float transformed_block_radius =
			volume_transform.basis.xform(Vector3(block_radius, block_radius, block_radius)).length();

	// Distance beyond which it is safe to drop a block without risking to block LOD subdivision.
	// This does not depend on viewer's view distance, but on LOD precision instead.
	// TODO Should `data_block_size` be used here? Should it be mesh_block_size instead?
	dep.drop_distance_squared = math::squared(2.f * transformed_block_radius *
			VoxelEngine::get_octree_lod_block_region_extent(octree_lod_distance, data_block_size));
}

// This is only if we want to cache voxel data
static void request_block_generate(VolumeID volume_id, unsigned int data_block_size,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelData> &data,
		Vector3i block_pos, int lod, std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data,
		const Transform3D &volume_transform, float lod_distance, std::shared_ptr<AsyncDependencyTracker> tracker,
		bool allow_drop, BufferedTaskScheduler &task_scheduler) {
	//
	CRASH_COND(data_block_size > 255);
	CRASH_COND(stream_dependency == nullptr);

	// We should not have done this request in the first place if both stream and generator are null
	ERR_FAIL_COND(stream_dependency->generator.is_null());

	GenerateBlockTask *task = memnew(GenerateBlockTask);
	task->volume_id = volume_id;
	task->position = block_pos;
	task->lod = lod;
	task->block_size = data_block_size;
	task->stream_dependency = stream_dependency;
	task->tracker = tracker;
	task->drop_beyond_max_distance = allow_drop;
	task->data = data;

	init_sparse_octree_priority_dependency(task->priority_dependency, block_pos, lod, data_block_size,
			shared_viewers_data, volume_transform, lod_distance);

	task_scheduler.push_main_task(task);
}

// Used only when streaming block by block
static void request_block_load(VolumeID volume_id, unsigned int data_block_size,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelData> &data,
		Vector3i block_pos, int lod, bool request_instances,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		const VoxelLodTerrainUpdateData::Settings &settings, BufferedTaskScheduler &task_scheduler) {
	//
	CRASH_COND(data_block_size > 255);
	CRASH_COND(stream_dependency == nullptr);

	if (stream_dependency->stream.is_valid()) {
		PriorityDependency priority_dependency;
		init_sparse_octree_priority_dependency(priority_dependency, block_pos, lod, data_block_size,
				shared_viewers_data, volume_transform, settings.lod_distance);

		LoadBlockDataTask *task = memnew(LoadBlockDataTask(volume_id, block_pos, lod, data_block_size,
				request_instances, stream_dependency, priority_dependency, settings.cache_generated_blocks));

		task_scheduler.push_io_task(task);

	} else if (settings.cache_generated_blocks) {
		// Directly generate the block without checking the stream.
		request_block_generate(volume_id, data_block_size, stream_dependency, data, block_pos, lod, shared_viewers_data,
				volume_transform, settings.lod_distance, nullptr, true, task_scheduler);

	} else {
		ZN_PRINT_WARNING("Requesting a block load when it should not have been necessary");
	}
}

static void send_block_data_requests(VolumeID volume_id,
		Span<const VoxelLodTerrainUpdateData::BlockLocation> blocks_to_load,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelData> &data,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, unsigned int data_block_size,
		bool request_instances, const Transform3D &volume_transform,
		const VoxelLodTerrainUpdateData::Settings &settings, BufferedTaskScheduler &task_scheduler) {
	//
	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const VoxelLodTerrainUpdateData::BlockLocation loc = blocks_to_load[i];
		request_block_load(volume_id, data_block_size, stream_dependency, data, loc.position, loc.lod,
				request_instances, shared_viewers_data, volume_transform, settings, task_scheduler);
	}
}

static void apply_block_data_requests_as_empty(Span<const VoxelLodTerrainUpdateData::BlockLocation> blocks_to_load,
		VoxelData &data, VoxelLodTerrainUpdateData::State &state) {
	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const VoxelLodTerrainUpdateData::BlockLocation loc = blocks_to_load[i];
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[loc.lod];
		{
			MutexLock mlock(lod.loading_blocks_mutex);
			lod.loading_blocks.erase(loc.position);
		}
		{
			VoxelDataBlock empty_block(loc.lod);
			data.try_set_block(loc.position, empty_block);
		}
	}
}

static void request_voxel_block_save(VolumeID volume_id, std::shared_ptr<VoxelBufferInternal> &voxels,
		Vector3i block_pos, int lod, std::shared_ptr<StreamingDependency> &stream_dependency,
		unsigned int data_block_size, BufferedTaskScheduler &task_scheduler) {
	//
	CRASH_COND(stream_dependency == nullptr);
	ERR_FAIL_COND(stream_dependency->stream.is_null());

	SaveBlockDataTask *task =
			memnew(SaveBlockDataTask(volume_id, block_pos, lod, data_block_size, voxels, stream_dependency, nullptr));

	// No priority data, saving doesn't need sorting.

	task_scheduler.push_io_task(task);
}

void VoxelLodTerrainUpdateTask::send_block_save_requests(VolumeID volume_id,
		Span<VoxelData::BlockToSave> blocks_to_save, std::shared_ptr<StreamingDependency> &stream_dependency,
		unsigned int data_block_size, BufferedTaskScheduler &task_scheduler) {
	for (unsigned int i = 0; i < blocks_to_save.size(); ++i) {
		VoxelData::BlockToSave &b = blocks_to_save[i];
		ZN_PRINT_VERBOSE(format("Requesting save of block {} lod {}", b.position, b.lod_index));
		request_voxel_block_save(
				volume_id, b.voxels, b.position, b.lod_index, stream_dependency, data_block_size, task_scheduler);
	}
}

static void send_mesh_requests(VolumeID volume_id, VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelData> &data_ptr,
		std::shared_ptr<MeshingDependency> meshing_dependency,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		BufferedTaskScheduler &task_scheduler) {
	//
	ZN_PROFILE_SCOPE();

	ZN_ASSERT(data_ptr != nullptr);
	const VoxelData &data = *data_ptr;

	const int data_block_size = data.get_block_size();
	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int render_to_data_factor = mesh_block_size / data_block_size;
	const unsigned int lod_count = data.get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		for (unsigned int bi = 0; bi < lod.blocks_pending_update.size(); ++bi) {
			ZN_PROFILE_SCOPE();
			const Vector3i mesh_block_pos = lod.blocks_pending_update[bi];

			auto mesh_block_it = lod.mesh_map_state.map.find(mesh_block_pos);
			// A block must have been allocated before we ask for a mesh update
			ZN_ASSERT_CONTINUE(mesh_block_it != lod.mesh_map_state.map.end());
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;
			// All blocks we get here must be in the scheduled state
			ZN_ASSERT_CONTINUE(mesh_block.state == VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT);

			// Get block and its neighbors
			// VoxelEngine::BlockMeshInput mesh_request;
			// mesh_request.render_block_position = mesh_block_pos;
			// mesh_request.lod = lod_index;

			// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
			MeshBlockTask *task = memnew(MeshBlockTask);
			task->volume_id = volume_id;
			task->mesh_block_position = mesh_block_pos;
			task->lod_index = lod_index;
			task->lod_hint = true;
			task->meshing_dependency = meshing_dependency;
			task->data_block_size = data_block_size;
			task->data = data_ptr;
			task->collision_hint = settings.collision_enabled;
			task->detail_texture_settings = settings.detail_texture_settings;
			task->detail_texture_generator_override = settings.detail_texture_generator_override;
			task->virtual_texture_generator_override_begin_lod_index =
					settings.virtual_texture_generator_override_begin_lod_index;
			task->virtual_texture_use_gpu = settings.virtual_textures_use_gpu;

			// Don't update a virtual texture if one update is already processing
			if (settings.detail_texture_settings.enabled &&
					lod_index >= settings.detail_texture_settings.begin_lod_index &&
					mesh_block.virtual_texture_state != VoxelLodTerrainUpdateData::VIRTUAL_TEXTURE_PENDING) {
				mesh_block.virtual_texture_state = VoxelLodTerrainUpdateData::VIRTUAL_TEXTURE_PENDING;
				task->require_virtual_texture = true;
			}

			const Box3i data_box =
					Box3i(render_to_data_factor * mesh_block_pos, Vector3iUtil::create(render_to_data_factor))
							.padded(1);

			// Iteration order matters for thread access.
			// The array also implicitly encodes block position due to the convention being used,
			// so there is no need to also include positions in the request
			data.get_blocks_with_voxel_data(data_box, lod_index, to_span(task->blocks));
			task->blocks_count = Vector3iUtil::get_volume(data_box.size);

			// TODO There is inconsistency with coordinates sent to this function.
			// Sometimes we send data block coordinates, sometimes we send mesh block coordinates. They aren't always
			// the same, it might cause issues in priority sorting?
			init_sparse_octree_priority_dependency(task->priority_dependency, task->mesh_block_position,
					task->lod_index, mesh_block_size, shared_viewers_data, volume_transform, settings.lod_distance);

			task_scheduler.push_main_task(task);

			mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_SENT;
		}

		lod.blocks_pending_update.clear();
	}
}

// Generates all non-present blocks in preparation for an edit.
// This function schedules one parallel task for every block.
// The returned tracker may be polled to detect when it is complete.
static std::shared_ptr<AsyncDependencyTracker> preload_boxes_async(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelData> data_ptr,
		Span<const Box3i> voxel_boxes, Span<IThreadedTask *> next_tasks, VolumeID volume_id,
		std::shared_ptr<StreamingDependency> &stream_dependency,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		BufferedTaskScheduler &task_scheduler) {
	ZN_PROFILE_SCOPE();

	ZN_ASSERT(data_ptr != nullptr);
	VoxelData &data = *data_ptr;

	ZN_ASSERT_RETURN_V_MSG(
			data.is_streaming_enabled() == false, nullptr, "This function can only be used in full load mode");

	struct TaskArguments {
		Vector3i block_pos;
		unsigned int lod_index;
	};

	std::vector<TaskArguments> todo;

	const unsigned int data_block_size = data.get_block_size();
	const unsigned int lod_count = data.get_lod_count();

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		for (unsigned int box_index = 0; box_index < voxel_boxes.size(); ++box_index) {
			ZN_PROFILE_SCOPE_NAMED("Box");

			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			const Box3i voxel_box = voxel_boxes[box_index];
			const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

			// ZN_PRINT_VERBOSE(String("Preloading box {0} at lod {1}")
			// 						.format(varray(block_box.to_string(), lod_index)));

			static thread_local std::vector<Vector3i> tls_missing;
			tls_missing.clear();
			data.get_missing_blocks(block_box, lod_index, tls_missing);

			if (tls_missing.size() > 0) {
				MutexLock mlock(lod.loading_blocks_mutex);
				for (const Vector3i &missing_bpos : tls_missing) {
					if (!lod.has_loading_block(missing_bpos)) {
						todo.push_back(TaskArguments{ missing_bpos, lod_index });
						lod.loading_blocks.insert(missing_bpos);
					}
				}
			}
		}
	}

	ZN_PRINT_VERBOSE(format("Preloading boxes with {} tasks", todo.size()));

	std::shared_ptr<AsyncDependencyTracker> tracker = nullptr;

	// TODO `next_tasks` is executed in parallel. But since they can be edits, may we do them in sequence?

	if (todo.size() > 0) {
		ZN_PROFILE_SCOPE_NAMED("Posting requests");

		// Only create the tracker if we actually are creating tasks. If we still create it,
		// no task will take ownership of it, so if it is not stored after this function returns,
		// it would destroy `next_tasks`.

		// This may first run the generation tasks, and then the edits
		tracker = make_shared_instance<AsyncDependencyTracker>(
				todo.size(), next_tasks, [](Span<IThreadedTask *> p_next_tasks) {
					VoxelEngine::get_singleton().push_async_tasks(p_next_tasks);
				});

		for (unsigned int i = 0; i < todo.size(); ++i) {
			const TaskArguments args = todo[i];
			request_block_generate(volume_id, data_block_size, stream_dependency, data_ptr, args.block_pos,
					args.lod_index, shared_viewers_data, volume_transform, settings.lod_distance, tracker, false,
					task_scheduler);
		}

	} else if (next_tasks.size() > 0) {
		// Nothing to preload, we may schedule `next_tasks` right now
		VoxelEngine::get_singleton().push_async_tasks(next_tasks);
	}

	return tracker;
}

static void process_async_edits(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelData> &data, VolumeID volume_id,
		std::shared_ptr<StreamingDependency> &stream_dependency,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		BufferedTaskScheduler &task_scheduler) {
	ZN_PROFILE_SCOPE();

	if (state.running_async_edits.size() == 0) {
		// Schedule all next edits when the previous ones are done

		std::vector<Box3i> boxes_to_preload;
		std::vector<IThreadedTask *> tasks_to_schedule;
		std::shared_ptr<AsyncDependencyTracker> last_tracker = nullptr;

		for (unsigned int edit_index = 0; edit_index < state.pending_async_edits.size(); ++edit_index) {
			VoxelLodTerrainUpdateData::AsyncEdit &edit = state.pending_async_edits[edit_index];
			CRASH_COND(edit.task_tracker->has_next_tasks());

			// Not sure if worth doing, I don't think tasks can be aborted before even being scheduled.
			if (edit.task_tracker->is_aborted()) {
				ZN_PRINT_VERBOSE("Aborted async edit");
				memdelete(edit.task);
				continue;
			}

			boxes_to_preload.push_back(edit.box);
			tasks_to_schedule.push_back(edit.task);
			state.running_async_edits.push_back(
					VoxelLodTerrainUpdateData::RunningAsyncEdit{ edit.task_tracker, edit.box });
		}

		if (boxes_to_preload.size() > 0) {
			preload_boxes_async(state, settings, data, to_span_const(boxes_to_preload), to_span(tasks_to_schedule),
					volume_id, stream_dependency, shared_viewers_data, volume_transform, task_scheduler);
		}

		state.pending_async_edits.clear();
	}
}

static void process_changed_generated_areas(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, unsigned int lod_count) {
	const unsigned int mesh_block_size = 1 << settings.mesh_block_size_po2;

	MutexLock lock(state.changed_generated_areas_mutex);
	if (state.changed_generated_areas.size() == 0) {
		return;
	}

	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		for (auto box_it = state.changed_generated_areas.begin(); box_it != state.changed_generated_areas.end();
				++box_it) {
			const Box3i &voxel_box = *box_it;
			const Box3i bbox = voxel_box.padded(1).downscaled(mesh_block_size << lod_index);

			// TODO If there are cached generated blocks, they need to be re-cached or removed

			RWLockRead rlock(lod.mesh_map_state.map_lock);

			bbox.for_each_cell_zxy([&lod](const Vector3i bpos) {
				auto block_it = lod.mesh_map_state.map.find(bpos);
				if (block_it != lod.mesh_map_state.map.end()) {
					VoxelLodTerrainUpdateTask::schedule_mesh_update(block_it->second, bpos, lod.blocks_pending_update);
				}
			});
		}
	}

	state.changed_generated_areas.clear();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void VoxelLodTerrainUpdateTask::run(ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();

	struct SetCompleteOnScopeExit {
		std::atomic_bool &_complete;
		SetCompleteOnScopeExit(std::atomic_bool &b) : _complete(b) {}
		~SetCompleteOnScopeExit() {
			_complete = true;
		}
	};

	CRASH_COND(_update_data == nullptr);
	CRASH_COND(_data == nullptr);
	CRASH_COND(_streaming_dependency == nullptr);
	CRASH_COND(_shared_viewers_data == nullptr);

	VoxelLodTerrainUpdateData &update_data = *_update_data;
	VoxelLodTerrainUpdateData::State &state = update_data.state;
	const VoxelLodTerrainUpdateData::Settings &settings = update_data.settings;
	VoxelData &data = *_data;
	Ref<VoxelGenerator> generator = _streaming_dependency->generator;
	Ref<VoxelStream> stream = _streaming_dependency->stream;
	ProfilingClock profiling_clock;
	ProfilingClock profiling_clock_total;

	// TODO This is not a good name, "streaming" has several meanings
	const bool stream_enabled = (stream.is_valid() || generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || settings.run_stream_in_editor);

	const unsigned int lod_count = data.get_lod_count();

	for (unsigned int lod_index = 0; lod_index < state.lods.size(); ++lod_index) {
		const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
		CRASH_COND(lod.mesh_blocks_to_unload.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_update_transitions.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_activate.size() != 0);
		CRASH_COND(lod.mesh_blocks_to_deactivate.size() != 0);
	}

	SetCompleteOnScopeExit scoped_complete(update_data.task_is_complete);

	CRASH_COND_MSG(update_data.task_is_complete, "Expected only one update task to run on a given volume");
	MutexLock mutex_lock(update_data.completion_mutex);

	// Update pending LOD data modifications due to edits.
	// These are deferred from edits so we can batch them.
	// It has to happen first because blocks can be unloaded afterwards.
	// This is also what causes meshes to update after edits.
	flush_pending_lod_edits(state, data, 1 << settings.mesh_block_size_po2);

	// Other mesh updates
	process_changed_generated_areas(state, settings, lod_count);

	static thread_local std::vector<VoxelData::BlockToSave> data_blocks_to_save;
	static thread_local std::vector<VoxelLodTerrainUpdateData::BlockLocation> data_blocks_to_load;
	data_blocks_to_load.clear();

	profiling_clock.restart();
	{
		// Unload data blocks falling out of block region extent
		if (data.is_streaming_enabled()) {
			process_unload_data_blocks_sliding_box(
					state, data, _viewer_pos, data_blocks_to_save, stream.is_valid(), settings);
		}

		// Unload mesh blocks falling out of block region extent
		process_unload_mesh_blocks_sliding_box(state, _viewer_pos, settings, data);

		// Create and remove octrees in a grid around the viewer.
		// Mesh blocks drive the loading of voxel data and visuals.
		process_octrees_sliding_box(state, _viewer_pos, settings, data);

		state.stats.blocked_lods = 0;

		// Find which blocks we need to load and see, within each octree
		if (stream_enabled) {
			process_octrees_fitting(state, settings, data, _viewer_pos, data_blocks_to_load);
		}
	}
	state.stats.time_detect_required_blocks = profiling_clock.restart();

	BufferedTaskScheduler &task_scheduler = BufferedTaskScheduler::get_for_current_thread();

	process_async_edits(state, settings, _data, _volume_id, _streaming_dependency, _shared_viewers_data,
			_volume_transform, task_scheduler);

	profiling_clock.restart();
	{
		ZN_PROFILE_SCOPE_NAMED("IO requests");
		// It's possible the user didn't set a stream yet, or it is turned off
		if (stream_enabled) {
			const unsigned int data_block_size = data.get_block_size();

			if (stream.is_null() && !settings.cache_generated_blocks) {
				// TODO Optimization: not ideal because a bit delayed. It requires a second update cycle for meshes to
				// get requested. We could instead set those empty blocks right away instead of putting them in that
				// list, but it's simpler code for now.
				apply_block_data_requests_as_empty(to_span(data_blocks_to_load), data, state);

			} else {
				send_block_data_requests(_volume_id, to_span(data_blocks_to_load), _streaming_dependency, _data,
						_shared_viewers_data, data_block_size, _request_instances, _volume_transform, settings,
						task_scheduler);
			}

			send_block_save_requests(
					_volume_id, to_span(data_blocks_to_save), _streaming_dependency, data_block_size, task_scheduler);
		}
		data_blocks_to_load.clear();
		data_blocks_to_save.clear();
	}
	state.stats.time_io_requests = profiling_clock.restart();

	// TODO When no mesher is assigned, mesh requests are still accumulated but not being sent. A better way to support
	// this is by allowing voxels-only/mesh-less viewers, similar to VoxelTerrain
	if (_meshing_dependency->mesher.is_valid()) {
		send_mesh_requests(_volume_id, state, settings, _data, _meshing_dependency, _shared_viewers_data,
				_volume_transform, task_scheduler);
	}

	task_scheduler.flush();

	state.stats.time_mesh_requests = profiling_clock.restart();

	state.stats.time_total = profiling_clock.restart();
}

} // namespace zylann::voxel
