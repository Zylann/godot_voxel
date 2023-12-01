#include "../../storage/voxel_data.h"
#include "../../util/math/conv.h"
#include "voxel_lod_terrain_update_data.h"
#include "voxel_lod_terrain_update_task.h"

namespace zylann::voxel {

void process_unload_data_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, VoxelData &data,
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

void process_unload_mesh_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, Vector3 p_viewer_pos,
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
	const Box3i prev_box = state.octree_streaming.last_octree_region_box;

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
				std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::iterator it =
						state.octree_streaming.lod_octrees.find(pos);
				if (it == state.octree_streaming.lod_octrees.end()) {
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

				state.octree_streaming.lod_octrees.erase(it);

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
				CRASH_COND(state.octree_streaming.lod_octrees.find(pos) != state.octree_streaming.lod_octrees.end());

				// Create new octree
				// TODO Use ObjectPool to store them, deletion won't be cheap
				std::pair<std::map<Vector3i, VoxelLodTerrainUpdateData::OctreeItem>::iterator, bool> p =
						state.octree_streaming.lod_octrees.insert({ pos, VoxelLodTerrainUpdateData::OctreeItem() });
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

		state.octree_streaming.force_update_octrees_next_update = true;
	}

	state.octree_streaming.last_octree_region_box = new_box;
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

bool check_block_loaded_and_meshed(VoxelLodTerrainUpdateData::State &state,
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

void process_octrees_fitting(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, VoxelData &data, Vector3 p_viewer_pos,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load) {
	//
	ZN_PROFILE_SCOPE();

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int octree_leaf_node_size = mesh_block_size;
	const unsigned int lod_count = data.get_lod_count();

	const bool force_update_octrees = state.octree_streaming.force_update_octrees_next_update;
	state.octree_streaming.force_update_octrees_next_update = false;

	// Octrees may not need to update every frame under certain conditions
	if (!state.octree_streaming.had_blocked_octree_nodes_previous_update && !force_update_octrees &&
			p_viewer_pos.distance_squared_to(Vector3(state.octree_streaming.local_viewer_pos_previous_octree_update)) <
					math::squared(octree_leaf_node_size / 2)) {
		return;
	}

	state.octree_streaming.local_viewer_pos_previous_octree_update = p_viewer_pos;

	const float lod_distance_octree_space = settings.lod_distance / octree_leaf_node_size;

	unsigned int blocked_octree_nodes = 0;

	// Off by one bit: second bit is LOD0, first bit is unused
	uint32_t lods_to_update_transitions = 0;

	// TODO Optimization: Maintain a vector to make iteration faster?
	for (auto octree_it = state.octree_streaming.lod_octrees.begin();
			octree_it != state.octree_streaming.lod_octrees.end(); ++octree_it) {
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
	state.octree_streaming.had_blocked_octree_nodes_previous_update = blocked_octree_nodes > 0;

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

void process_octree_streaming(VoxelLodTerrainUpdateData::State &state, VoxelData &data, Vector3 viewer_pos,
		std::vector<VoxelData::BlockToSave> &data_blocks_to_save,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, Ref<VoxelStream> stream, bool stream_enabled) {
	ZN_PROFILE_SCOPE();

	// Unload data blocks falling out of block region extent.
	// We only unload data if data streaming is enabled. Otherwise it's always loaded.
	if (data.is_streaming_enabled()) {
		process_unload_data_blocks_sliding_box(
				state, data, viewer_pos, data_blocks_to_save, stream.is_valid(), settings);
	}

	// Unload mesh blocks falling out of block region extent
	process_unload_mesh_blocks_sliding_box(state, viewer_pos, settings, data);

	// Create and remove octrees in a grid around the viewer.
	// Mesh blocks drive the loading of voxel data and visuals.
	process_octrees_sliding_box(state, viewer_pos, settings, data);

	state.stats.blocked_lods = 0;

	// Find which blocks we need to load and see, within each octree
	if (stream_enabled) {
		process_octrees_fitting(state, settings, data, viewer_pos, data_blocks_to_load);
	}
}

} // namespace zylann::voxel
