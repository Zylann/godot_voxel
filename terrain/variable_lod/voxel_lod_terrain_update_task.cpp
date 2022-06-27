#include "voxel_lod_terrain_update_task.h"
#include "../../server/generate_block_task.h"
#include "../../server/load_block_data_task.h"
#include "../../server/mesh_block_task.h"
#include "../../server/save_block_data_task.h"
#include "../../server/voxel_server.h"
#include "../../util/container_funcs.h"
#include "../../util/dstack.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/profiling_clock.h"
#include "../../util/string_funcs.h"

namespace zylann::voxel {

void VoxelLodTerrainUpdateTask::flush_pending_lod_edits(VoxelLodTerrainUpdateData::State &state, VoxelDataLodMap &data,
		Ref<VoxelGenerator> generator, bool full_load_mode, const int mesh_block_size) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	// Propagates edits performed so far to other LODs.
	// These LODs must be currently in memory, otherwise terrain data will miss it.
	// This is currently ensured by the fact we load blocks in a "pyramidal" way,
	// i.e there is no way for a block to be loaded if its parent LOD isn't loaded already.
	// In the future we may implement storing of edits to be applied later if blocks can't be found.

	const int data_block_size = data.lods[0].map.get_block_size();
	const int data_block_size_po2 = data.lods[0].map.get_block_size_pow2();
	const int data_to_mesh_factor = mesh_block_size / data_block_size;
	const unsigned int lod_count = data.lod_count;

	static thread_local FixedArray<std::vector<Vector3i>, constants::MAX_LOD> tls_blocks_to_process_per_lod;

	// Make sure LOD0 gets updates even if _lod_count is 1
	VoxelLodTerrainUpdateData::Lod &lod0 = state.lods[0];
	{
		// Consume scheduled positions from LOD0
		std::vector<Vector3i> &dst_lod0 = tls_blocks_to_process_per_lod[0];

		MutexLock lock(state.blocks_pending_lodding_lod0_mutex);

		// Not sure if could just use `=`? What would std::vector do with capacity?
		dst_lod0.resize(state.blocks_pending_lodding_lod0.size());
		memcpy(dst_lod0.data(), state.blocks_pending_lodding_lod0.data(), dst_lod0.size() * sizeof(Vector3i));

		// Make sure LOD0 has its list cleared, because in case there is only 1 LOD,
		// the chain of updates will not be entered
		state.blocks_pending_lodding_lod0.clear();
	}
	{
		VoxelDataLodMap::Lod &data_lod0 = data.lods[0];
		RWLockRead rlock(data_lod0.map_lock);

		std::vector<Vector3i> &blocks_pending_lodding_lod0 = tls_blocks_to_process_per_lod[0];

		for (unsigned int i = 0; i < blocks_pending_lodding_lod0.size(); ++i) {
			const Vector3i data_block_pos = blocks_pending_lodding_lod0[i];
			VoxelDataBlock *data_block = data_lod0.map.get_block(data_block_pos);
			ERR_CONTINUE(data_block == nullptr);
			data_block->set_needs_lodding(false);

			const Vector3i mesh_block_pos = math::floordiv(data_block_pos, data_to_mesh_factor);
			auto mesh_block_it = lod0.mesh_map_state.map.find(mesh_block_pos);
			if (mesh_block_it != lod0.mesh_map_state.map.end()) {
				// If a mesh exists here, it will need an update.
				// If there is no mesh, it will probably get created later when we come closer to it
				schedule_mesh_update(mesh_block_it->second, mesh_block_pos, lod0.blocks_pending_update);
			}
		}
	}

	const int half_bs = data_block_size >> 1;

	// Process downscales upwards in pairs of consecutive LODs.
	// This ensures we don't process multiple times the same blocks.
	// Only LOD0 is editable at the moment, so we'll downscale from there
	for (uint8_t dst_lod_index = 1; dst_lod_index < lod_count; ++dst_lod_index) {
		const uint8_t src_lod_index = dst_lod_index - 1;
		std::vector<Vector3i> &src_lod_blocks_to_process = tls_blocks_to_process_per_lod[src_lod_index];
		std::vector<Vector3i> &dst_lod_blocks_to_process = tls_blocks_to_process_per_lod[dst_lod_index];

		VoxelLodTerrainUpdateData::Lod &dst_lod = state.lods[dst_lod_index];

		VoxelDataLodMap::Lod &src_data_lod = data.lods[src_lod_index];
		RWLockRead src_data_lod_map_rlock(src_data_lod.map_lock);

		VoxelDataLodMap::Lod &dst_data_lod = data.lods[dst_lod_index];
		// TODO Could take long locking this, we may generate things first and assign to the map at the end.
		// Besides, in per-block streaming mode, it is not needed because blocks are supposed to be present
		RWLockRead wlock(dst_data_lod.map_lock);

		for (unsigned int i = 0; i < src_lod_blocks_to_process.size(); ++i) {
			const Vector3i src_bpos = src_lod_blocks_to_process[i];
			const Vector3i dst_bpos = src_bpos >> 1;

			VoxelDataBlock *src_block = src_data_lod.map.get_block(src_bpos);
			VoxelDataBlock *dst_block = dst_data_lod.map.get_block(dst_bpos);

			src_block->set_needs_lodding(false);

			if (dst_block == nullptr) {
				if (full_load_mode) {
					// TODO Doing this on the main thread can be very demanding and cause a stall.
					// We should find a way to make it asynchronous, not need mips, or not edit outside viewers area.
					std::shared_ptr<VoxelBufferInternal> voxels = make_shared_instance<VoxelBufferInternal>();
					voxels->create(Vector3iUtil::create(data_block_size));
					VoxelGenerator::VoxelQueryData q{ //
						*voxels, //
						dst_bpos << (dst_lod_index + data_block_size_po2), //
						dst_lod_index
					};
					if (generator.is_valid()) {
						ZN_PROFILE_SCOPE_NAMED("Generate");
						generator->generate_block(q);
					}
					data.modifiers.apply(
							q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << dst_lod_index));

					dst_block = dst_data_lod.map.set_block_buffer(dst_bpos, voxels, true);

				} else {
					ERR_PRINT(String("Destination block {0} not found when cascading edits on LOD {1}")
									  .format(varray(dst_bpos, dst_lod_index)));
					continue;
				}
			}

			// The block and its lower LOD indices are expected to be available.
			// Otherwise it means the function was called too late?
			ZN_ASSERT(src_block != nullptr);
			//ZN_ASSERT(dst_block != nullptr);
			// The block should have voxels if it has been edited or mipped.
			ZN_ASSERT(src_block->has_voxels());

			{
				const Vector3i mesh_block_pos = math::floordiv(dst_bpos, data_to_mesh_factor);
				auto mesh_block_it = dst_lod.mesh_map_state.map.find(mesh_block_pos);
				if (mesh_block_it != dst_lod.mesh_map_state.map.end()) {
					schedule_mesh_update(mesh_block_it->second, mesh_block_pos, dst_lod.blocks_pending_update);
				}
				// If there is no mesh, it will probably get created later when we come closer to it
			}

			dst_block->set_modified(true);

			if (dst_lod_index != lod_count - 1 && !dst_block->get_needs_lodding()) {
				dst_block->set_needs_lodding(true);
				dst_lod_blocks_to_process.push_back(dst_bpos);
			}

			const Vector3i rel = src_bpos - (dst_bpos << 1);

			// Update lower LOD
			// This must always be done after an edit before it gets saved, otherwise LODs won't match and it will look
			// ugly.
			// TODO Optimization: try to narrow to edited region instead of taking whole block
			{
				ZN_PROFILE_SCOPE_NAMED("Downscale");
				RWLockRead rlock(src_block->get_voxels().get_lock());
				src_block->get_voxels().downscale_to(
						dst_block->get_voxels(), Vector3i(), src_block->get_voxels_const().get_size(), rel * half_bs);
			}
		}

		src_lod_blocks_to_process.clear();
		// No need to clear the last list because we never add blocks to it
	}

	//	uint64_t time_spent = profiling_clock.restart();
	//	if (time_spent > 10) {
	//		print_line(String("Took {0} us to update lods").format(varray(time_spent)));
	//	}
}

struct BeforeUnloadDataAction {
	std::vector<VoxelLodTerrainUpdateData::BlockToSave> &blocks_to_save;
	const Vector3i bpos;
	bool save;

	void operator()(VoxelDataBlock &block) {
		// Save if modified
		// TODO Don't ask for save if the stream doesn't support it!
		if (save && block.is_modified()) {
			//print_line(String("Scheduling save for block {0}").format(varray(block->position.to_vec3())));
			VoxelLodTerrainUpdateData::BlockToSave b;
			// We don't copy since the block will be unloaded anyways.
			// If a modified block has no voxels, it is equivalent to removing the block from the stream
			if (block.has_voxels()) {
				b.voxels = block.get_voxels_shared();
			}
			b.position = bpos;
			b.lod = block.get_lod_index();
			blocks_to_save.push_back(b);
		}
	}
};

static void unload_data_block_no_lock(VoxelLodTerrainUpdateData::Lod &lod, VoxelDataLodMap::Lod &data_lod,
		Vector3i block_pos, std::vector<VoxelLodTerrainUpdateData::BlockToSave> &blocks_to_save, bool can_save) {
	ZN_PROFILE_SCOPE();

	data_lod.map.remove_block(block_pos, BeforeUnloadDataAction{ blocks_to_save, block_pos, can_save });

	//print_line(String("Unloading data block {0} lod {1}").format(varray(block_pos.to_vec3(), lod_index)));
	MutexLock lock(lod.loading_blocks_mutex);
	lod.loading_blocks.erase(block_pos);

	// if (_instancer != nullptr) {
	// 	_instancer->on_block_exit(block_pos, lod_index);
	// }

	// No need to remove things from blocks_pending_load,
	// This vector is filled and cleared immediately in the main process.
	// It is a member only to re-use its capacity memory over frames.
}

static void process_unload_data_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, VoxelDataLodMap &data,
		Vector3 p_viewer_pos, std::vector<VoxelLodTerrainUpdateData::BlockToSave> &blocks_to_save, bool can_save,
		const VoxelLodTerrainUpdateData::Settings &settings) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box data unload");
	// TODO Could it actually be enough to have a rolling update on all blocks?

	// This should be the same distance relatively to each LOD
	const int data_block_size = data.lods[0].map.get_block_size();
	const int data_block_size_po2 = data.lods[0].map.get_block_size_pow2();
	const int data_block_region_extent =
			VoxelServer::get_octree_lod_block_region_extent(settings.lod_distance, data_block_size);

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;

	const int lod_count = data.lod_count;

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
				settings.bounds_in_voxels.pos >> block_size_po2, //
				settings.bounds_in_voxels.size >> block_size_po2);

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
			VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			RWLockWrite wlock(data_lod.map_lock);
			prev_box.difference(new_box, [&lod, &data_lod, &blocks_to_save, can_save](Box3i out_of_range_box) {
				out_of_range_box.for_each_cell([&lod, &data_lod, &blocks_to_save, can_save](Vector3i pos) {
					//print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
					unload_data_block_no_lock(lod, data_lod, pos, blocks_to_save, can_save);
				});
			});
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
		const VoxelLodTerrainUpdateData::Settings &settings) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box mesh unload");
	// TODO Could it actually be enough to have a rolling update on all blocks?

	// This should be the same distance relatively to each LOD
	const int mesh_block_size_po2 = settings.mesh_block_size_po2;
	const int mesh_block_size = 1 << mesh_block_size_po2;
	const int mesh_block_region_extent =
			VoxelServer::get_octree_lod_block_region_extent(settings.lod_distance, mesh_block_size);

	// Ignore largest lod because it can extend a little beyond due to the view distance setting.
	// Instead, those blocks are unloaded by the octree forest management.
	// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
	for (int lod_index = settings.lod_count - 2; lod_index >= 0; --lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		unsigned int block_size_po2 = mesh_block_size_po2 + lod_index;
		const Vector3i viewer_block_pos_within_lod = math::floor_to_int(p_viewer_pos) >> block_size_po2;

		const Box3i bounds_in_blocks = Box3i( //
				settings.bounds_in_voxels.pos >> block_size_po2, //
				settings.bounds_in_voxels.size >> block_size_po2);

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
					//print_line(String("Immerge {0}").format(varray(pos.to_vec3())));
					//unload_mesh_block(pos, lod_index);
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
		const VoxelLodTerrainUpdateData::Settings &settings) {
	ZN_PROFILE_SCOPE_NAMED("Sliding box octrees");
	// TODO Investigate if multi-octree can produce cracks in the terrain (so far I haven't noticed)

	const unsigned int mesh_block_size_po2 = settings.mesh_block_size_po2;
	const unsigned int octree_size_po2 = LodOctree::get_octree_size_po2(mesh_block_size_po2, settings.lod_count);
	const unsigned int octree_size = 1 << octree_size_po2;
	const unsigned int octree_region_extent = 1 + settings.view_distance_voxels / (1 << octree_size_po2);

	const Vector3i viewer_octree_pos =
			(math::floor_to_int(p_viewer_pos) + Vector3iUtil::create(octree_size / 2)) >> octree_size_po2;

	const Box3i bounds_in_octrees = settings.bounds_in_voxels.downscaled(octree_size);

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

		ExitAction exit_action{ state, settings.lod_count };
		EnterAction enter_action{ state, settings.lod_count };
		{
			ZN_PROFILE_SCOPE_NAMED("Unload octrees");

			const unsigned int last_lod_index = settings.lod_count - 1;
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

static void add_transition_update(VoxelLodTerrainUpdateData::MeshBlockState &block, Vector3i bpos,
		std::vector<Vector3i> &blocks_pending_transition_update) {
	if (!block.pending_transition_update) {
		blocks_pending_transition_update.push_back(bpos);
		block.pending_transition_update = true;
	}
}

static void add_transition_updates_around(VoxelLodTerrainUpdateData::Lod &lod, Vector3i block_pos,
		std::vector<Vector3i> &blocks_pending_transition_update) {
	//
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		const Vector3i npos = block_pos + Cube::g_side_normals[dir];
		auto nblock_it = lod.mesh_map_state.map.find(npos);

		if (nblock_it != lod.mesh_map_state.map.end()) {
			add_transition_update(nblock_it->second, npos, blocks_pending_transition_update);
		}
	}
	// TODO If a block appears at lod, neighbor blocks at lod-1 need to be updated.
	// or maybe get_transition_mask needs a different approach that also looks at higher lods?
}

void try_schedule_loading_with_neighbors_no_lock(VoxelLodTerrainUpdateData::State &state, VoxelDataLodMap &data,
		const Vector3i &p_data_block_pos, uint8_t lod_index,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &blocks_to_load, const Box3i &bounds_in_voxels) {
	//
	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
	const VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];

	const int p = data_lod.map.get_block_size_pow2() + lod_index;

	const int bound_min_x = bounds_in_voxels.pos.x >> p;
	const int bound_min_y = bounds_in_voxels.pos.y >> p;
	const int bound_min_z = bounds_in_voxels.pos.z >> p;
	const int bound_max_x = (bounds_in_voxels.pos.x + bounds_in_voxels.size.x) >> p;
	const int bound_max_y = (bounds_in_voxels.pos.y + bounds_in_voxels.size.y) >> p;
	const int bound_max_z = (bounds_in_voxels.pos.z + bounds_in_voxels.size.z) >> p;

	const int min_x = math::max(p_data_block_pos.x - 1, bound_min_x);
	const int min_y = math::max(p_data_block_pos.y - 1, bound_min_y);
	const int min_z = math::max(p_data_block_pos.z - 1, bound_min_z);
	const int max_x = math::min(p_data_block_pos.x + 2, bound_max_x);
	const int max_y = math::min(p_data_block_pos.y + 2, bound_max_y);
	const int max_z = math::min(p_data_block_pos.z + 2, bound_max_z);

	// Not locking here, we assume it's done by the caller
	//RWLockRead rlock(data_lod.map_lock);

	Vector3i bpos;
	MutexLock lock(lod.loading_blocks_mutex);
	for (bpos.y = min_y; bpos.y < max_y; ++bpos.y) {
		for (bpos.z = min_z; bpos.z < max_z; ++bpos.z) {
			for (bpos.x = min_x; bpos.x < max_x; ++bpos.x) {
				const VoxelDataBlock *block = data_lod.map.get_block(bpos);

				if (block == nullptr) {
					if (!lod.has_loading_block(bpos)) {
						blocks_to_load.push_back({ bpos, lod_index });
						lod.loading_blocks.insert(bpos);
					}
				}
			}
		}
	}
}

inline bool check_block_sizes(int data_block_size, int mesh_block_size) {
	return (data_block_size == 16 || data_block_size == 32) && (mesh_block_size == 16 || mesh_block_size == 32) &&
			mesh_block_size >= data_block_size;
}

bool check_block_mesh_updated(VoxelLodTerrainUpdateData::State &state, VoxelDataLodMap &data,
		VoxelLodTerrainUpdateData::MeshBlockState &mesh_block, Vector3i mesh_block_pos, uint8_t lod_index,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings) {
	//ZN_PROFILE_SCOPE();

	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	const VoxelLodTerrainUpdateData::MeshState mesh_state = mesh_block.state;

	switch (mesh_state) {
		case VoxelLodTerrainUpdateData::MESH_NEVER_UPDATED:
		case VoxelLodTerrainUpdateData::MESH_NEED_UPDATE: {
			const int mesh_block_size = 1 << settings.mesh_block_size_po2;
			const int data_block_size = data.lods[0].map.get_block_size();
#ifdef DEBUG_ENABLED
			ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif
			// Find data block neighbors positions
			const int factor = mesh_block_size / data_block_size;
			const Vector3i data_block_pos0 = factor * mesh_block_pos;
			const Box3i data_box(data_block_pos0 - Vector3i(1, 1, 1), Vector3iUtil::create(factor) + Vector3i(2, 2, 2));
			const Box3i bounds = settings.bounds_in_voxels.downscaled(data_block_size);
			FixedArray<Vector3i, 56> neighbor_positions;
			unsigned int neighbor_positions_count = 0;
			data_box.for_inner_outline([bounds, &neighbor_positions, &neighbor_positions_count](Vector3i pos) {
				if (bounds.contains(pos)) {
					neighbor_positions[neighbor_positions_count] = pos;
					++neighbor_positions_count;
				}
			});

			bool surrounded = true;
			if (settings.full_load_mode == false) {
				const VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
				// Check if neighbors are loaded
				RWLockRead rlock(data_lod.map_lock);
				// TODO Optimization: could put in a temp vector and insert in one go after the loop?
				MutexLock lock(lod.loading_blocks_mutex);
				for (unsigned int i = 0; i < neighbor_positions_count; ++i) {
					const Vector3i npos = neighbor_positions[i];
					if (!data_lod.map.has_block(npos)) {
						// That neighbor is not loaded
						surrounded = false;
						if (!lod.has_loading_block(npos)) {
							// Schedule loading for that neighbor
							blocks_to_load.push_back({ npos, lod_index });
							lod.loading_blocks.insert(npos);
						}
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
		const VoxelLodTerrainUpdateData::Settings &settings, VoxelDataLodMap &data, const Vector3i &p_mesh_block_pos,
		uint8_t lod_index, std::vector<VoxelLodTerrainUpdateData::BlockLocation> &blocks_to_load) {
	//
	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int data_block_size = data.lods[0].map.get_block_size();

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(!check_block_sizes(data_block_size, mesh_block_size), false);
#endif

	if (settings.full_load_mode == false) {
		// We want to know everything about the data intersecting this mesh block.
		// This is not known in advance when we stream it, it has to be requested.
		// When not streaming, `block == null` is the same as `!block->has_voxels()` so we wouldn't need to enter here.

		VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];

		if (mesh_block_size > data_block_size) {
			const int factor = mesh_block_size / data_block_size;
			const Vector3i data_block_pos0 = p_mesh_block_pos * factor;

			bool loaded = true;

			RWLockRead rlock(data_lod.map_lock);
			for (int z = 0; z < factor; ++z) {
				for (int x = 0; x < factor; ++x) {
					for (int y = 0; y < factor; ++y) {
						const Vector3i data_block_pos(data_block_pos0 + Vector3i(x, y, z));
						VoxelDataBlock *data_block = data_lod.map.get_block(data_block_pos);

						if (data_block == nullptr) {
							loaded = false;
							// TODO Optimization: this iterates too many blocks, if we ask for 8 blocks in an octant.
							try_schedule_loading_with_neighbors_no_lock(
									state, data, data_block_pos, lod_index, blocks_to_load, settings.bounds_in_voxels);
						}
					}
				}
			}

			if (!loaded) {
				return false;
			}

		} else if (mesh_block_size == data_block_size) {
			const Vector3i data_block_pos = p_mesh_block_pos;
			RWLockRead rlock(data_lod.map_lock);
			VoxelDataBlock *block = data_lod.map.get_block(data_block_pos);
			if (block == nullptr) {
				try_schedule_loading_with_neighbors_no_lock(
						state, data, data_block_pos, lod_index, blocks_to_load, settings.bounds_in_voxels);
				return false;
			}
		}
	}

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

uint8_t VoxelLodTerrainUpdateTask::get_transition_mask(
		const VoxelLodTerrainUpdateData::State &state, Vector3i block_pos, int lod_index, unsigned int lod_count) {
	uint8_t transition_mask = 0;

	if (lod_index + 1 >= static_cast<int>(lod_count)) {
		return transition_mask;
	}

	const VoxelLodTerrainUpdateData::Lod &lower_lod = state.lods[lod_index + 1];
	const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

	const Vector3i lower_pos = block_pos >> 1;
	const Vector3i upper_pos = block_pos << 1;

	// Based on octree rules, and the fact it must have run before, check neighbor blocks of same LOD:
	// If one is missing or not visible, it means either of the following:
	// - The neighbor at lod+1 is visible or not loaded (there must be a transition)
	// - The neighbor at lod-1 is visible (no transition)

	uint8_t visible_neighbors_of_same_lod = 0;
	for (int dir = 0; dir < Cube::SIDE_COUNT; ++dir) {
		Vector3i npos = block_pos + Cube::g_side_normals[dir];

		auto nblock_it = lod.mesh_map_state.map.find(npos);

		if (nblock_it != lod.mesh_map_state.map.end() && nblock_it->second.active) {
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
		const VoxelLodTerrainUpdateData::Settings &settings, VoxelDataLodMap &data, Vector3 p_viewer_pos,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load) {
	//
	ZN_PROFILE_SCOPE();

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int octree_leaf_node_size = mesh_block_size;

	const bool force_update_octrees = state.force_update_octrees_next_update;
	state.force_update_octrees_next_update = false;

	// Octrees may not need to update every frame under certain conditions
	if (!state.had_blocked_octree_nodes_previous_update && !force_update_octrees &&
			p_viewer_pos.distance_squared_to(Vector3(state.local_viewer_pos_previous_octree_update)) <
					math::squared(octree_leaf_node_size / 2)) {
		return;
	}

	state.local_viewer_pos_previous_octree_update = p_viewer_pos;

	static thread_local FixedArray<std::vector<Vector3i>, constants::MAX_LOD>
			tls_blocks_pending_transition_update_per_lod;
	//static thread_local FixedArray<std::vector<Vector3i>, constants::MAX_LOD> tls_mesh_blocks_to_add_per_lod;

	for (unsigned int i = 0; i < tls_blocks_pending_transition_update_per_lod.size(); ++i) {
		CRASH_COND(!tls_blocks_pending_transition_update_per_lod[i].empty());
		//CRASH_COND(!tls_mesh_blocks_to_add_per_lod[i].empty());
	}

	const float lod_distance_octree_space = settings.lod_distance / octree_leaf_node_size;

	unsigned int blocked_octree_nodes = 0;

	// TODO Maintain a vector to make iteration faster?
	for (auto octree_it = state.lod_octrees.begin(); octree_it != state.lod_octrees.end(); ++octree_it) {
		ZN_PROFILE_SCOPE();

		struct OctreeActions {
			VoxelLodTerrainUpdateData::State &state;
			const VoxelLodTerrainUpdateData::Settings &settings;
			VoxelDataLodMap &data;
			std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load;
			Vector3i block_offset_lod0;
			unsigned int blocked_count = 0;
			float lod_distance_octree_space;
			Vector3 viewer_pos_octree_space;

			void create_child(Vector3i node_pos, int lod_index, LodOctree::NodeData &data) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
				const Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				// Never show a child that hasn't been meshed, if we got here that would be a bug
				CRASH_COND(mesh_block_it == lod.mesh_map_state.map.end());
				CRASH_COND(mesh_block_it->second.state != VoxelLodTerrainUpdateData::MESH_UP_TO_DATE);

				//self->set_mesh_block_active(*block, true);
				lod.mesh_blocks_to_activate.push_back(bpos);
				mesh_block_it->second.active = true;
				add_transition_update(
						mesh_block_it->second, bpos, tls_blocks_pending_transition_update_per_lod[lod_index]);
				add_transition_updates_around(lod, bpos, tls_blocks_pending_transition_update_per_lod[lod_index]);
			}

			void destroy_child(Vector3i node_pos, int lod_index) {
				VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
				const Vector3i bpos = node_pos + (block_offset_lod0 >> lod_index);
				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

				if (mesh_block_it != lod.mesh_map_state.map.end()) {
					//self->set_mesh_block_active(*block, false);
					mesh_block_it->second.active = false;
					lod.mesh_blocks_to_deactivate.push_back(bpos);
					add_transition_updates_around(lod, bpos, tls_blocks_pending_transition_update_per_lod[lod_index]);
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
					//self->set_mesh_block_active(*block, true);
					mesh_block_it->second.active = true;
					lod.mesh_blocks_to_activate.push_back(bpos);
					add_transition_update(
							mesh_block_it->second, bpos, tls_blocks_pending_transition_update_per_lod[lod_index]);
					add_transition_updates_around(lod, bpos, tls_blocks_pending_transition_update_per_lod[lod_index]);
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
		const Vector3i block_offset_lod0 = block_pos_maxlod << (settings.lod_count - 1);
		const Vector3 relative_viewer_pos = p_viewer_pos - Vector3(mesh_block_size * block_offset_lod0);

		OctreeActions octree_actions{ //
			state, //
			settings, //
			data, //
			data_blocks_to_load, //
			block_offset_lod0, //
			0, //
			lod_distance_octree_space, //
			relative_viewer_pos / octree_leaf_node_size
		};
		VoxelLodTerrainUpdateData::OctreeItem &item = octree_it->second;
		item.octree.update(octree_actions);

		blocked_octree_nodes += octree_actions.blocked_count;
	}

	// Ideally, this stat should stabilize to zero.
	// If not, something in block management prevents LODs from properly show up and should be fixed.
	state.stats.blocked_lods = blocked_octree_nodes;
	state.had_blocked_octree_nodes_previous_update = blocked_octree_nodes > 0;

	{
		// In theory, blocks containing no surface have no reason to need a transition mask,
		// but when we get a new mesh update into a block that previously had no surface, we still need it.

		ZN_PROFILE_SCOPE_NAMED("Transition masks");
		for (unsigned int lod_index = 0; lod_index < tls_blocks_pending_transition_update_per_lod.size(); ++lod_index) {
			std::vector<Vector3i> &blocks_pending_transition_update =
					tls_blocks_pending_transition_update_per_lod[lod_index];

			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

			for (unsigned int i = 0; i < blocks_pending_transition_update.size(); ++i) {
				const Vector3i bpos = blocks_pending_transition_update[i];

				auto mesh_block_it = lod.mesh_map_state.map.find(bpos);
				CRASH_COND(mesh_block_it == lod.mesh_map_state.map.end());
				VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;

				if (mesh_block.active) {
					const uint8_t mask =
							VoxelLodTerrainUpdateTask::get_transition_mask(state, bpos, lod_index, settings.lod_count);
					mesh_block.transition_mask = mask;
					lod.mesh_blocks_to_update_transitions.push_back(
							VoxelLodTerrainUpdateData::TransitionUpdate{ bpos, mask });
				}

				mesh_block.pending_transition_update = false;
			}

			blocks_pending_transition_update.clear();
		}
	}
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
			VoxelServer::get_octree_lod_block_region_extent(octree_lod_distance, data_block_size));
}

// This is only if we want to cache voxel data
static void request_block_generate(uint32_t volume_id, unsigned int data_block_size,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelDataLodMap> &data,
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
static void request_block_load(uint32_t volume_id, unsigned int data_block_size,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelDataLodMap> &data,
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

static void send_block_data_requests(uint32_t volume_id,
		Span<const VoxelLodTerrainUpdateData::BlockLocation> blocks_to_load,
		std::shared_ptr<StreamingDependency> &stream_dependency, const std::shared_ptr<VoxelDataLodMap> &data,
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
		VoxelDataLodMap &data, VoxelLodTerrainUpdateData::State &state) {
	for (unsigned int i = 0; i < blocks_to_load.size(); ++i) {
		const VoxelLodTerrainUpdateData::BlockLocation loc = blocks_to_load[i];
		VoxelDataLodMap::Lod &data_lod = data.lods[loc.lod];
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[loc.lod];
		{
			MutexLock mlock(lod.loading_blocks_mutex);
			lod.loading_blocks.erase(loc.position);
		}
		{
			RWLockWrite wlock(data_lod.map_lock);
			data_lod.map.set_empty_block(loc.position, false);
		}
	}
}

static void request_voxel_block_save(uint32_t volume_id, std::shared_ptr<VoxelBufferInternal> &voxels,
		Vector3i block_pos, int lod, std::shared_ptr<StreamingDependency> &stream_dependency,
		unsigned int data_block_size, BufferedTaskScheduler &task_scheduler) {
	//
	CRASH_COND(stream_dependency == nullptr);
	ERR_FAIL_COND(stream_dependency->stream.is_null());

	SaveBlockDataTask *task =
			memnew(SaveBlockDataTask(volume_id, block_pos, lod, data_block_size, voxels, stream_dependency));

	// No priority data, saving doesnt need sorting

	task_scheduler.push_io_task(task);
}

void VoxelLodTerrainUpdateTask::send_block_save_requests(uint32_t volume_id,
		Span<VoxelLodTerrainUpdateData::BlockToSave> blocks_to_save,
		std::shared_ptr<StreamingDependency> &stream_dependency, unsigned int data_block_size,
		BufferedTaskScheduler &task_scheduler) {
	for (unsigned int i = 0; i < blocks_to_save.size(); ++i) {
		VoxelLodTerrainUpdateData::BlockToSave &b = blocks_to_save[i];
		ZN_PRINT_VERBOSE(format("Requesting save of block {} lod {}", b.position, b.lod));
		request_voxel_block_save(
				volume_id, b.voxels, b.position, b.lod, stream_dependency, data_block_size, task_scheduler);
	}
}

static void send_mesh_requests(uint32_t volume_id, VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelDataLodMap> &data_ptr,
		std::shared_ptr<MeshingDependency> meshing_dependency,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		BufferedTaskScheduler &task_scheduler) {
	//
	ZN_PROFILE_SCOPE_NAMED("Send mesh requests");

	CRASH_COND(data_ptr == nullptr);
	const VoxelDataLodMap &data = *data_ptr;

	const int data_block_size = data.lods[0].map.get_block_size();
	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int render_to_data_factor = mesh_block_size / data_block_size;

	for (unsigned int lod_index = 0; lod_index < settings.lod_count; ++lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		for (unsigned int bi = 0; bi < lod.blocks_pending_update.size(); ++bi) {
			ZN_PROFILE_SCOPE();
			const Vector3i mesh_block_pos = lod.blocks_pending_update[bi];

			auto mesh_block_it = lod.mesh_map_state.map.find(mesh_block_pos);
			// A block must have been allocated before we ask for a mesh update
			ERR_CONTINUE(mesh_block_it == lod.mesh_map_state.map.end());
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;
			// All blocks we get here must be in the scheduled state
			ERR_CONTINUE(mesh_block.state != VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT);

			// Get block and its neighbors
			// VoxelServer::BlockMeshInput mesh_request;
			// mesh_request.render_block_position = mesh_block_pos;
			// mesh_request.lod = lod_index;

			// We'll allocate this quite often. If it becomes a problem, it should be easy to pool.
			MeshBlockTask *task = memnew(MeshBlockTask);
			task->volume_id = volume_id;
			task->position = mesh_block_pos;
			task->lod_index = lod_index;
			task->lod_hint = true;
			task->meshing_dependency = meshing_dependency;
			task->data_block_size = data_block_size;
			task->data = data_ptr;

			const Box3i data_box =
					Box3i(render_to_data_factor * mesh_block_pos, Vector3iUtil::create(render_to_data_factor))
							.padded(1);

			const VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			RWLockRead rlock(data_lod.map_lock);

			// Iteration order matters for thread access.
			// The array also implicitely encodes block position due to the convention being used,
			// so there is no need to also include positions in the request
			task->blocks_count = 0;
			data_box.for_each_cell_zxy([task, &data_lod](Vector3i data_block_pos) {
				const VoxelDataBlock *nblock = data_lod.map.get_block(data_block_pos);
				// The block can actually be null on some occasions. Not sure yet if it's that bad
				//CRASH_COND(nblock == nullptr);
				if (nblock != nullptr && nblock->has_voxels()) {
					task->blocks[task->blocks_count] = nblock->get_voxels_shared();
				}
				++task->blocks_count;
			});

			init_sparse_octree_priority_dependency(task->priority_dependency, task->position, task->lod_index,
					mesh_block_size, shared_viewers_data, volume_transform, settings.lod_distance);

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
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelDataLodMap> data_ptr,
		Span<const Box3i> voxel_boxes, Span<IThreadedTask *> next_tasks, uint32_t volume_id,
		std::shared_ptr<StreamingDependency> &stream_dependency,
		std::shared_ptr<PriorityDependency::ViewersData> &shared_viewers_data, const Transform3D &volume_transform,
		BufferedTaskScheduler &task_scheduler) {
	ZN_PROFILE_SCOPE();

	ERR_FAIL_COND_V_MSG(settings.full_load_mode == false, nullptr, "This function can only be used in full load mode");

	struct TaskArguments {
		Vector3i block_pos;
		unsigned int lod_index;
	};

	std::vector<TaskArguments> todo;

	ZN_ASSERT(data_ptr != nullptr);
	VoxelDataLodMap &data = *data_ptr;
	const unsigned int data_block_size = data.lods[0].map.get_block_size();

	for (unsigned int lod_index = 0; lod_index < settings.lod_count; ++lod_index) {
		for (unsigned int box_index = 0; box_index < voxel_boxes.size(); ++box_index) {
			ZN_PROFILE_SCOPE_NAMED("Box");

			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			const Box3i voxel_box = voxel_boxes[box_index];
			const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

			// ZN_PRINT_VERBOSE(String("Preloading box {0} at lod {1}")
			// 						.format(varray(block_box.to_string(), lod_index)));

			const VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			RWLockRead rlock(data_lod.map_lock);
			MutexLock lock(lod.loading_blocks_mutex);

			block_box.for_each_cell([&lod, &data_lod, lod_index, &todo](Vector3i block_pos) {
				if (!data_lod.map.has_block(block_pos) && !lod.has_loading_block(block_pos)) {
					todo.push_back({ block_pos, lod_index });
					lod.loading_blocks.insert(block_pos);
				}
			});
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
					VoxelServer::get_singleton().push_async_tasks(p_next_tasks);
				});

		for (unsigned int i = 0; i < todo.size(); ++i) {
			const TaskArguments args = todo[i];
			request_block_generate(volume_id, data_block_size, stream_dependency, data_ptr, args.block_pos,
					args.lod_index, shared_viewers_data, volume_transform, settings.lod_distance, tracker, false,
					task_scheduler);
		}

	} else if (next_tasks.size() > 0) {
		// Nothing to preload, we may schedule `next_tasks` right now
		VoxelServer::get_singleton().push_async_tasks(next_tasks);
	}

	return tracker;
}

static void process_async_edits(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const std::shared_ptr<VoxelDataLodMap> &data,
		uint32_t volume_id, std::shared_ptr<StreamingDependency> &stream_dependency,
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

			// Not sure if worth doing, I dont think tasks can be aborted before even being scheduled
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

static void process_changed_generated_areas(
		VoxelLodTerrainUpdateData::State &state, const VoxelLodTerrainUpdateData::Settings &settings) {
	const unsigned int mesh_block_size = 1 << settings.mesh_block_size_po2;

	MutexLock lock(state.changed_generated_areas_mutex);
	if (state.changed_generated_areas.size() == 0) {
		return;
	}

	for (unsigned int lod_index = 0; lod_index < settings.lod_count; ++lod_index) {
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
	VoxelDataLodMap &data = *_data;
	Ref<VoxelGenerator> generator = _streaming_dependency->generator;
	Ref<VoxelStream> stream = _streaming_dependency->stream;
	ProfilingClock profiling_clock;
	ProfilingClock profiling_clock_total;

	// TODO This is not a good name, "streaming" has several meanings
	const bool stream_enabled = (stream.is_valid() || generator.is_valid()) &&
			(Engine::get_singleton()->is_editor_hint() == false || settings.run_stream_in_editor);

	CRASH_COND(data.lod_count != update_data.settings.lod_count);

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
	flush_pending_lod_edits(state, data, generator, settings.full_load_mode, 1 << settings.mesh_block_size_po2);

	// Other mesh updates
	process_changed_generated_areas(state, settings);

	static thread_local std::vector<VoxelLodTerrainUpdateData::BlockToSave> data_blocks_to_save;
	static thread_local std::vector<VoxelLodTerrainUpdateData::BlockLocation> data_blocks_to_load;
	data_blocks_to_load.clear();

	profiling_clock.restart();
	{
		// Unload data blocks falling out of block region extent
		if (update_data.settings.full_load_mode == false) {
			process_unload_data_blocks_sliding_box(
					state, data, _viewer_pos, data_blocks_to_save, stream.is_valid(), settings);
		}

		// Unload mesh blocks falling out of block region extent
		process_unload_mesh_blocks_sliding_box(state, _viewer_pos, settings);

		// Create and remove octrees in a grid around the viewer.
		// Mesh blocks drive the loading of voxel data and visuals.
		process_octrees_sliding_box(state, _viewer_pos, settings);

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
			const unsigned int data_block_size = data.lods[0].map.get_block_size();

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

	// TODO Don't request meshes if there is no mesher
	send_mesh_requests(_volume_id, state, settings, _data, _meshing_dependency, _shared_viewers_data, _volume_transform,
			task_scheduler);

	task_scheduler.flush();

	state.stats.time_mesh_requests = profiling_clock.restart();

	state.stats.time_total = profiling_clock.restart();
}

} // namespace zylann::voxel
