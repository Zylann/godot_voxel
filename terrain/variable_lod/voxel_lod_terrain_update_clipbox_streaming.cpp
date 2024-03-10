#include "voxel_lod_terrain_update_clipbox_streaming.h"
#include "../../util/containers/std_unordered_set.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "voxel_lod_terrain_update_task.h"

// #include <fstream>

namespace zylann::voxel {

// Note:
// This streaming method allows every LOD to load in parallel, even before meshes are ready. That means if a data block
// is loaded somewhere and gets edited, there is no guarantee its parent LODs are loaded! It can be made more likely,
// but not guaranteed.
// Hopefully however, this should not be a problem if LODs are just cosmetic representations of
// LOD0. If an edit happens at LOD0 and sibling chunks are present, they can be used to produce the parent LOD.
// Alternatively, LOD updates could wait. In worst case, parent LODs will not update.

// TODO Octree streaming was polling constantly, but clipbox isn't. So if a task is dropped due to being too far away,
// it might cause a chunk hole or blocked lods, because it won't be requested again...
// Either we should handle "dropped" responses and retrigger if still needed (as we did before), or we could track every
// loading tasks with a shared boolean owned by both the task and the requester, which the requester sets to false if
// it's not needed anymore, and otherwise doesn't get cancelled.

namespace {

bool find_index(Span<const std::pair<ViewerID, VoxelEngine::Viewer>> viewers, ViewerID id, unsigned int &out_index) {
	for (unsigned int i = 0; i < viewers.size(); ++i) {
		if (viewers[i].first == id) {
			out_index = i;
			return true;
		}
	}
	return false;
}

bool contains(Span<const std::pair<ViewerID, VoxelEngine::Viewer>> viewers, ViewerID id) {
	unsigned int _unused;
	return find_index(viewers, id, _unused);
}

bool find_index(Span<const VoxelLodTerrainUpdateData::PairedViewer> viewers, ViewerID id, unsigned int &out_index) {
	for (unsigned int i = 0; i < viewers.size(); ++i) {
		if (viewers[i].id == id) {
			out_index = i;
			return true;
		}
	}
	return false;
}

Box3i get_base_box_in_chunks(Vector3i viewer_position_voxels, int distance_voxels, int chunk_size, bool make_even) {
	// Get min and max positions
	Vector3i minp = viewer_position_voxels - Vector3iUtil::create(distance_voxels);
	Vector3i maxp = viewer_position_voxels +
			Vector3iUtil::create(distance_voxels
					// When distance is a multiple of chunk size, we should be able to get a consistent box size,
					// however without that +1 there are still very specific coordinates that makes the box shrink due
					// to rounding
					+ 1);

	// Convert to chunk coordinates
	minp = math::floordiv(minp, chunk_size);
	maxp = math::ceildiv(maxp, chunk_size);

	if (make_even) {
		// Round to be even outwards (partly required for subdivision rule)
		// TODO Maybe there is a more clever way to do this
		minp = math::floordiv(minp, 2) * 2;
		maxp = math::ceildiv(maxp, 2) * 2;
	}

	return Box3i::from_min_max(minp, maxp);
}

// Gets the smallest box a parent LOD must have in order to keep respecting the neighboring rule
Box3i get_minimal_box_for_parent_lod(Box3i child_lod_box, bool make_even) {
	const int min_pad = 1;
	// Note, subdivision rule enforces the child box position and size to be even, so it won't round to
	// zero when converted to the parent LOD's coordinate system.
	Box3i min_box = Box3i(child_lod_box.pos >> 1, child_lod_box.size >> 1)
							// Enforce neighboring rule by padding boxes outwards by a minimum amount,
							// so there is at least N chunks in the current LOD between LOD+1 and LOD-1
							.padded(min_pad);

	if (make_even) {
		// Make sure it stays even to respect subdivision rule, rounding outwards
		min_box = min_box.downscaled(2).scaled(2);
	}

	return min_box;
}

Box3i enforce_neighboring_rule(Box3i box, const Box3i &child_lod_box, bool make_even) {
	const Box3i min_box = get_minimal_box_for_parent_lod(child_lod_box, make_even);
	box.merge_with(min_box);
	return box;
}

inline int get_lod_distance_in_mesh_chunks(float lod_distance_in_voxels, int mesh_block_size) {
	return math::max(static_cast<int>(Math::ceil(lod_distance_in_voxels)) / mesh_block_size, 1);
}

// Compute distance in chunks relative to the current LOD, between the viewer and the end of that LOD
int get_relative_lod_distance_in_chunks(int lod_index, int lod_count, int lod0_distance_in_chunks,
		int lodn_distance_in_chunks, int lod_chunk_size, int max_view_distance_voxels) {
	int ld;
	if (lod_index == 0) {
		// First LOD uses dedicated distance
		ld = lod0_distance_in_chunks;
	} else {
		// Following LODs use another distance.
		// The returned distance is relative to chunks of the current LOD so we divide LOD0 distance rather than
		// multiplying LODN distance
		ld = (lod0_distance_in_chunks >> lod_index) + lodn_distance_in_chunks;
	}
	if (lod_index == lod_count - 1) {
		// Last LOD may extend all the way to max view distance if possible
		ld = math::max(ld, math::ceildiv(max_view_distance_voxels, lod_chunk_size));
	}
	return ld;
}

void process_viewers(VoxelLodTerrainUpdateData::ClipboxStreamingState &cs,
		const VoxelLodTerrainUpdateData::Settings &volume_settings, unsigned int lod_count,
		Span<const std::pair<ViewerID, VoxelEngine::Viewer>> viewers, const Transform3D &volume_transform,
		Box3i volume_bounds_in_voxels, int data_block_size_po2, bool can_mesh,
		// Ordered by ascending index in paired viewers list
		StdVector<unsigned int> &unpaired_viewers_to_remove) {
	ZN_PROFILE_SCOPE();

	// Destroyed viewers
	for (size_t paired_viewer_index = 0; paired_viewer_index < cs.paired_viewers.size(); ++paired_viewer_index) {
		VoxelLodTerrainUpdateData::PairedViewer &pv = cs.paired_viewers[paired_viewer_index];

		if (!contains(viewers, pv.id)) {
			ZN_PRINT_VERBOSE(format("Detected destroyed viewer {} in VoxelLodTerrain", pv.id));

			// Interpret removal as nullified view distance so the same code handling loading of blocks
			// will be used to unload those viewed by this viewer.
			// We'll actually remove unpaired viewers in a second pass.
			pv.state.view_distance_voxels = 0;

			// Also update boxes, they won't be updated since the viewer has been removed.
			// Assign prev state, otherwise in some cases resetting boxes would make them equal to prev state,
			// therefore causing no unload
			pv.prev_state = pv.state;

			for (unsigned int lod_index = 0; lod_index < pv.state.data_box_per_lod.size(); ++lod_index) {
				pv.state.data_box_per_lod[lod_index] = Box3i();
			}
			for (unsigned int lod_index = 0; lod_index < pv.state.mesh_box_per_lod.size(); ++lod_index) {
				pv.state.mesh_box_per_lod[lod_index] = Box3i();
			}

			unpaired_viewers_to_remove.push_back(paired_viewer_index);
		}
	}

	// TODO Pair/Unpair viewers as they intersect volume bounds

	const Transform3D world_to_local_transform = volume_transform.affine_inverse();

	// Note, this does not support non-uniform scaling
	// TODO There is probably a better way to do this
	const float view_distance_scale = world_to_local_transform.basis.xform(Vector3(1, 0, 0)).length();

	const int data_block_size = 1 << data_block_size_po2;

	const int mesh_block_size = 1 << volume_settings.mesh_block_size_po2;
	const int mesh_to_data_factor = mesh_block_size / data_block_size;

	const int lod0_distance_in_mesh_chunks =
			get_lod_distance_in_mesh_chunks(volume_settings.lod_distance, mesh_block_size);
	const int lodn_distance_in_mesh_chunks =
			get_lod_distance_in_mesh_chunks(volume_settings.secondary_lod_distance, mesh_block_size);

	// Data chunks are driven by mesh chunks, because mesh needs data
	const int lod0_distance_in_data_chunks = lod0_distance_in_mesh_chunks * mesh_to_data_factor;
	const int lodn_distance_in_data_chunks = lodn_distance_in_mesh_chunks * mesh_to_data_factor;

	// const Box3i volume_bounds_in_data_blocks = volume_bounds_in_voxels.downscaled(1 << data_block_size_po2);
	// const Box3i volume_bounds_in_mesh_blocks = volume_bounds_in_voxels.downscaled(1 << mesh_block_size_po2);

	// New viewers and existing viewers.
	// Removed viewers won't be iterated but are still paired until later.
	for (const std::pair<ViewerID, VoxelEngine::Viewer> &viewer_and_id : viewers) {
		const ViewerID viewer_id = viewer_and_id.first;
		const VoxelEngine::Viewer &viewer = viewer_and_id.second;

		unsigned int paired_viewer_index;
		if (!find_index(to_span_const(cs.paired_viewers), viewer_id, paired_viewer_index)) {
			// New viewer
			VoxelLodTerrainUpdateData::PairedViewer pv;
			pv.id = viewer_id;
			paired_viewer_index = cs.paired_viewers.size();
			cs.paired_viewers.push_back(pv);
			ZN_PRINT_VERBOSE(format("Pairing viewer {} to VoxelLodTerrain", viewer_id));
		}

		VoxelLodTerrainUpdateData::PairedViewer &paired_viewer = cs.paired_viewers[paired_viewer_index];

		// Move current state to be the previous state
		paired_viewer.prev_state = paired_viewer.state;

		const int view_distance_voxels =
				static_cast<int>(static_cast<float>(viewer.view_distance) * view_distance_scale);
		paired_viewer.state.view_distance_voxels =
				math::min(view_distance_voxels, static_cast<int>(volume_settings.view_distance_voxels));

		// The last LOD should extend at least up to view distance. It must also be at least the distance specified by
		// "lod distance"
		// const int last_lod_mesh_block_size = mesh_block_size << (lod_count - 1);
		// const int last_lod_distance_in_mesh_chunks =
		// 		math::max(math::ceildiv(paired_viewer.state.view_distance_voxels, last_lod_mesh_block_size),
		// 				lod_distance_in_mesh_chunks);
		// const int last_lod_distance_in_data_chunks = last_lod_mesh_block_size * mesh_to_data_factor;

		const Vector3 local_position = world_to_local_transform.xform(viewer.world_position);

		paired_viewer.state.local_position_voxels = math::floor_to_int(local_position);
		paired_viewer.state.requires_collisions = viewer.require_collisions && can_mesh;
		paired_viewer.state.requires_visuals = viewer.require_visuals && can_mesh;

		// Viewers can request any box they like, but they must follow these rules:
		// - Boxes of parent LODs must contain child boxes (when converted into world coordinates)
		// - Mesh boxes that have a parent LOD must have an even size and even position, in order to support subdivision
		// - Mesh boxes must be contained within data boxes, in order to guarantee that meshes have access to consistent
		//   voxel blocks and their neighbors

		// TODO The root LOD should not need to have even size.
		// However if we do that, one corner case is when LOD count is changed in the editor, it might cause errors
		// since every LOD is assumed to have an even size when handling subdivisions

		// Update data and mesh boxes
		if (paired_viewer.state.requires_collisions || paired_viewer.state.requires_visuals) {
			// Meshes are required

			for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
				const int lod_mesh_block_size_po2 = volume_settings.mesh_block_size_po2 + lod_index;
				const int lod_mesh_block_size = 1 << lod_mesh_block_size_po2;

				const Box3i volume_bounds_in_mesh_blocks = volume_bounds_in_voxels.downscaled(lod_mesh_block_size);

				const int ld = get_relative_lod_distance_in_chunks(lod_index, lod_count, lod0_distance_in_mesh_chunks,
						lodn_distance_in_mesh_chunks, lod_mesh_block_size, paired_viewer.state.view_distance_voxels);

				// Box3i new_mesh_box = get_lod_box_in_chunks(
				// 		paired_viewer.state.local_position_voxels, ld, volume_settings.mesh_block_size_po2, lod_index);

				// Make min and max coordinates even in child LODs, to respect subdivision rule.
				// Root LOD doesn't need to respect that.
				const bool even_coordinates_required = (lod_index != lod_count - 1);

				Box3i new_mesh_box = get_base_box_in_chunks(paired_viewer.state.local_position_voxels,
						// Making sure that distance is a multiple of chunk size, for consistent box size
						ld * lod_mesh_block_size, lod_mesh_block_size, even_coordinates_required);

				if (lod_index > 0) {
					const Box3i &child_box = paired_viewer.state.mesh_box_per_lod[lod_index - 1];
					new_mesh_box = enforce_neighboring_rule(new_mesh_box, child_box, even_coordinates_required);
				}

				// Clip last
				new_mesh_box.clip(volume_bounds_in_mesh_blocks);

				paired_viewer.state.mesh_box_per_lod[lod_index] = new_mesh_box;
			}

			// TODO We should have a flag server side to force data boxes to be based on mesh boxes, even though the
			// server might not actually need meshes. That would help the server to provide data chunks to clients,
			// which need them for visual meshes

			// Data boxes must be based on mesh boxes so the right data chunks are loaded to make the corresponding
			// meshes (also including the tweaks we do to mesh boxes to enforce the neighboring rule)
			for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
				const unsigned int lod_data_block_size_po2 = data_block_size_po2 + lod_index;

				// Should be correct as long as bounds size is a multiple of the biggest LOD chunk
				const Box3i volume_bounds_in_data_blocks = Box3i( //
						volume_bounds_in_voxels.pos >> lod_data_block_size_po2, //
						volume_bounds_in_voxels.size >> lod_data_block_size_po2);

				// const int ld =
				// 		(lod_index == (lod_count - 1) ? lod_distance_in_data_chunks : last_lod_distance_in_data_chunks);

				// const Box3i new_data_box =
				// 		get_lod_box_in_chunks(paired_viewer.state.local_position_voxels, lod_distance_in_data_chunks,
				// 				data_block_size_po2, lod_index)
				// 				// To account for meshes requiring neighbor data chunks.
				// 				// It technically breaks the subdivision rule (where every parent block always has 8
				// 				// children), but it should only matter in areas where meshes must actually spawn
				// 				.padded(1)
				// 				.clipped(volume_bounds_in_data_blocks);

				const Box3i &mesh_box = paired_viewer.state.mesh_box_per_lod[lod_index];

				const Box3i data_box =
						Box3i(mesh_box.pos * mesh_to_data_factor, mesh_box.size * mesh_to_data_factor)
								// To account for meshes requiring neighbor data chunks.
								// It technically breaks the subdivision rule (where every parent block always has 8
								// children), but it should only matter in areas where meshes must actually spawn
								.padded(1)
								.clipped(volume_bounds_in_data_blocks);

				paired_viewer.state.data_box_per_lod[lod_index] = data_box;
			}

		} else {
			for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
				paired_viewer.state.mesh_box_per_lod[lod_index] = Box3i();
			}

			for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
				const int lod_data_block_size_po2 = data_block_size_po2 + lod_index;
				const int lod_data_block_size = 1 << lod_data_block_size_po2;

				// Should be correct as long as bounds size is a multiple of the biggest LOD chunk
				const Box3i volume_bounds_in_data_blocks = Box3i( //
						volume_bounds_in_voxels.pos >> lod_data_block_size_po2, //
						volume_bounds_in_voxels.size >> lod_data_block_size_po2);

				const int ld = get_relative_lod_distance_in_chunks(lod_index, lod_count, lod0_distance_in_data_chunks,
						lodn_distance_in_data_chunks, lod_data_block_size, paired_viewer.state.view_distance_voxels);

				const Box3i new_data_box = get_base_box_in_chunks(paired_viewer.state.local_position_voxels,
						// Making sure that distance is a multiple of chunk size, for consistent box size
						ld * lod_data_block_size, lod_data_block_size,
						// Make min and max coordinates even in child LODs, to respect subdivision rule.
						// Root LOD doesn't need to respect that,
						lod_index != lod_count - 1)
												   .clipped(volume_bounds_in_data_blocks);

				// const Box3i new_data_box = get_lod_box_in_chunks(paired_viewer.state.local_position_voxels,
				// 		lod_distance_in_data_chunks, data_block_size_po2, lod_index)
				// 								   .clipped(volume_bounds_in_data_blocks);

				paired_viewer.state.data_box_per_lod[lod_index] = new_data_box;
			}
		}
	}
}

void remove_unpaired_viewers(const StdVector<unsigned int> &unpaired_viewers_to_remove,
		StdVector<VoxelLodTerrainUpdateData::PairedViewer> &paired_viewers) {
	// Iterating backward so indexes of paired viewers that need removal will not change because of the removal itself
	for (auto it = unpaired_viewers_to_remove.rbegin(); it != unpaired_viewers_to_remove.rend(); ++it) {
		const unsigned int vi = *it;
		ZN_PRINT_VERBOSE(format("Unpairing viewer {} from VoxelLodTerrain", paired_viewers[vi].id));
		paired_viewers[vi] = paired_viewers.back();
		paired_viewers.pop_back();
	}
}

void add_loading_block(VoxelLodTerrainUpdateData::Lod &lod, Vector3i position, uint8_t lod_index,
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &blocks_to_load) {
	auto it = lod.loading_blocks.find(position);

	if (it == lod.loading_blocks.end()) {
		// First viewer to request it
		VoxelLodTerrainUpdateData::LoadingDataBlock new_loading_block;
		new_loading_block.viewers.add();
		new_loading_block.cancellation_token = TaskCancellationToken::create();

		lod.loading_blocks.insert({ position, new_loading_block });

		blocks_to_load.push_back(
				VoxelLodTerrainUpdateData::BlockToLoad{ VoxelLodTerrainUpdateData::BlockLocation{ position, lod_index },
						new_loading_block.cancellation_token });

	} else {
		// Already loaded
		it->second.viewers.add();
	}
}

void unreference_data_block_from_loading_lists(
		StdUnorderedMap<Vector3i, VoxelLodTerrainUpdateData::LoadingDataBlock> &loading_blocks,
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load, Vector3i bpos, unsigned int lod_index) {
	auto loading_block_it = loading_blocks.find(bpos);
	if (loading_block_it == loading_blocks.end()) {
		ZN_PRINT_VERBOSE("Request to unview a loading block that was never requested");
		// Not expected, but fine I guess
		return;
	}

	VoxelLodTerrainUpdateData::LoadingDataBlock &loading_block = loading_block_it->second;
	loading_block.viewers.remove();

	if (loading_block.viewers.get() == 0) {
		// No longer want to load it, no data box contains it

		if (loading_block.cancellation_token.is_valid()) {
			// Cancel loading task if still in queue
			loading_block.cancellation_token.cancel();
		}

		loading_blocks.erase(loading_block_it);

		// Also remove from blocks about to be added to the loading queue
		VoxelLodTerrainUpdateData::BlockLocation bloc{ bpos, static_cast<uint8_t>(lod_index) };
		for (size_t i = 0; i < data_blocks_to_load.size(); ++i) {
			if (data_blocks_to_load[i].loc == bloc) {
				data_blocks_to_load[i] = data_blocks_to_load.back();
				data_blocks_to_load.pop_back();
				// We don't touch the cancellation token since tasks haven't been spawned yet for
				// these
				break;
			}
		}
	}
}

void process_data_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, VoxelData &data,
		StdVector<VoxelData::BlockToSave> *blocks_to_save,
		// TODO We should be able to work in BOXES to load, it can help compressing network messages
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, int lod_count, bool can_load) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN_MSG(data.is_streaming_enabled(), "This function is not meant to run in full load mode");

	const int data_block_size_po2 = data.get_block_size_po2();
	const Box3i bounds_in_voxels = data.get_bounds();

	// const int mesh_to_data_factor = mesh_block_size / data_block_size;

	// const int lod_distance_in_mesh_chunks = get_lod_distance_in_mesh_chunks(settings.lod_distance, mesh_block_size);

	// // Data chunks are driven by mesh chunks, because mesh needs data
	// const int lod_distance_in_data_chunks = lod_distance_in_mesh_chunks * mesh_to_data_factor
	// 		// To account for the fact meshes need neighbor data chunks
	// 		+ 1;

	static thread_local StdVector<Vector3i> tls_missing_blocks;
	static thread_local StdVector<Vector3i> tls_found_blocks_positions;

#ifdef DEV_ENABLED
	Box3i debug_parent_box;
#endif

	for (const VoxelLodTerrainUpdateData::PairedViewer &paired_viewer : state.clipbox_streaming.paired_viewers) {
		// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
		for (int lod_index = lod_count - 1; lod_index >= 0; --lod_index) {
			ZN_PROFILE_SCOPE();
			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

			// Each LOD keeps a box of loaded blocks, and only some of the blocks will get polygonized.
			// The player can edit them so changes can be propagated to lower lods.

			const unsigned int lod_data_block_size_po2 = data_block_size_po2 + lod_index;

			// Should be correct as long as bounds size is a multiple of the biggest LOD chunk
			const Box3i bounds_in_data_blocks = Box3i( //
					bounds_in_voxels.pos >> lod_data_block_size_po2, //
					bounds_in_voxels.size >> lod_data_block_size_po2);

			// const Box3i new_data_box = get_lod_box_in_chunks(
			// 		viewer_pos_in_lod0_voxels, lod_distance_in_data_chunks, data_block_size_po2, lod_index)
			// 								   .clipped(bounds_in_data_blocks);

			const Box3i &new_data_box = paired_viewer.state.data_box_per_lod[lod_index];
			const Box3i &prev_data_box = paired_viewer.prev_state.data_box_per_lod[lod_index];

#ifdef DEV_ENABLED
			if (lod_index + 1 != lod_count) {
				const Box3i debug_parent_box_in_current_lod(debug_parent_box.pos << 1, debug_parent_box.size << 1);
				ZN_ASSERT(debug_parent_box_in_current_lod.contains(new_data_box));
			}
			debug_parent_box = new_data_box;
#endif

			// const Box3i prev_data_box = get_lod_box_in_chunks(
			// 		state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update,
			// 		state.clipbox_streaming.lod_distance_in_data_chunks_previous_update, data_block_size_po2, lod_index)
			// 									.clipped(bounds_in_data_blocks);

			if (!new_data_box.intersects(bounds_in_data_blocks) && !prev_data_box.intersects(bounds_in_data_blocks)) {
				// If this box doesn't intersect either now or before, there is no chance a smaller one will
				break;
			}

			if (prev_data_box != new_data_box) {
				// Detect blocks to load.
				if (can_load) {
					tls_missing_blocks.clear();

					new_data_box.difference(prev_data_box, [&data, lod_index](Box3i box_to_load) {
						data.view_area(box_to_load, lod_index, &tls_missing_blocks, nullptr, nullptr);
					});

					{
						ZN_PROFILE_SCOPE_NAMED("Add loading blocks");
						for (const Vector3i bpos : tls_missing_blocks) {
							add_loading_block(lod, bpos, lod_index, data_blocks_to_load);
						}
					}
				}

				// Detect blocks to unload
				{
					tls_missing_blocks.clear();
					tls_found_blocks_positions.clear();

					const unsigned int to_save_index0 = blocks_to_save != nullptr ? blocks_to_save->size() : 0;

					prev_data_box.difference(new_data_box, [&data, blocks_to_save, lod_index](Box3i box_to_remove) {
						data.unview_area(box_to_remove, lod_index, &tls_found_blocks_positions, &tls_missing_blocks,
								blocks_to_save);
					});

					if (blocks_to_save != nullptr && blocks_to_save->size() > to_save_index0) {
						add_unloaded_saving_blocks(lod, to_span(*blocks_to_save).sub(to_save_index0));
					}

					// Remove loading blocks regardless of refcount (those were loaded and had their refcount reach
					// zero)
					for (const Vector3i bpos : tls_found_blocks_positions) {
						// emit_data_block_unloaded(bpos);

						// TODO If they were loaded, why would they be in loading blocks?
						// Maybe to make sure they are not in here regardless
						lod.loading_blocks.erase(bpos);
					}

					// Remove refcount from loading blocks, and cancel loading if it reaches zero
					for (const Vector3i bpos : tls_missing_blocks) {
						unreference_data_block_from_loading_lists(
								lod.loading_blocks, data_blocks_to_load, bpos, lod_index);
					}
				}
			}

			// Turned this off because I don't remember why I added it. Keeping it in case a bug occurs that could
			// highlight why it was there.
			// Was originally added in 17c6b1f557c5abc447cb62c200afcff1298fadff
			// Perhaps that's in case there was updates pending in the list before we get here, so there needs to be
			// some way of cancelling them? But with clipbox logic and multiple viewers, that no longer works
#if 0
			// TODO Why do we do this here? Sounds like it should be done in the mesh clipbox logic
			{
				ZN_PROFILE_SCOPE_NAMED("Cancel updates");
				// Cancel mesh block updates that are not within the padded region
				// (since neighbors are always required to remesh)

				// TODO This might break at terrain borders
				const Box3i padded_new_box = new_data_box.padded(-1);
				Box3i mesh_box;
				if (mesh_block_size > data_block_size) {
					const int factor = mesh_block_size / data_block_size;
					mesh_box = padded_new_box.downscaled_inner(factor);
				} else {
					mesh_box = padded_new_box;
				}

				unordered_remove_if(lod.mesh_blocks_pending_update,
						[&lod, mesh_box](const VoxelLodTerrainUpdateData::MeshToUpdate &mtu) {
							if (mesh_box.contains(mtu.position)) {
								return false;
							} else {
								auto mesh_block_it = lod.mesh_map_state.map.find(mtu.position);
								if (mesh_block_it != lod.mesh_map_state.map.end()) {
									mesh_block_it->second.state = VoxelLodTerrainUpdateData::MESH_NEED_UPDATE;
								}
								return true;
							}
						});
			}
#endif

		} // for each lod
	} // for each viewer

	// state.clipbox_streaming.lod_distance_in_data_chunks_previous_update = lod_distance_in_data_chunks;
}

// TODO Copypasta from octree streaming file
VoxelLodTerrainUpdateData::MeshBlockState &insert_new(
		StdUnorderedMap<Vector3i, VoxelLodTerrainUpdateData::MeshBlockState> &mesh_map, Vector3i pos) {
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

inline Vector3i get_relative_child_position(unsigned int child_index) {
	return Vector3i( //
			(child_index & 1), //
			((child_index & 2) >> 1), //
			((child_index & 4) >> 2));
}

inline Vector3i get_child_position(Vector3i parent_position, unsigned int child_index) {
	return parent_position * 2 + get_relative_child_position(child_index);
}

// void hide_children_recursive(
// 		VoxelLodTerrainUpdateData::State &state, unsigned int parent_lod_index, Vector3i parent_cpos) {
// 	ZN_ASSERT_RETURN(parent_lod_index > 0);
// 	const unsigned int lod_index = parent_lod_index - 1;
// 	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

// 	for (unsigned int child_index = 0; child_index < 8; ++child_index) {
// 		const Vector3i cpos = get_child_position(parent_cpos, child_index);
// 		auto mesh_it = lod.mesh_map_state.map.find(cpos);

// 		if (mesh_it != lod.mesh_map_state.map.end()) {
// 			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;

// 			if (mesh_block.active) {
// 				mesh_block.active = false;
// 				lod.mesh_blocks_to_deactivate.push_back(cpos);

// 			} else if (lod_index > 0) {
// 				hide_children_recursive(state, lod_index, cpos);
// 			}
// 		}
// 	}
// }

inline void schedule_mesh_load(StdVector<VoxelLodTerrainUpdateData::MeshToUpdate> &update_list, Vector3i bpos,
		VoxelLodTerrainUpdateData::MeshBlockState &mesh_block, bool require_visual) {
	// ZN_PROFILE_SCOPE();

	if (mesh_block.update_list_index != -1) {
		// Update settings before the task is scheduled
		VoxelLodTerrainUpdateData::MeshToUpdate &u = update_list[mesh_block.update_list_index];
		u.require_visual |= require_visual;
	} else {
		TaskCancellationToken cancellation_token = TaskCancellationToken::create();
		mesh_block.update_list_index = update_list.size();
		update_list.push_back(VoxelLodTerrainUpdateData::MeshToUpdate{ bpos, cancellation_token, require_visual });
		mesh_block.cancellation_token = cancellation_token;
		// TODO `MESH_UPDATE_NOT_SENT` is now redundant with `update_list_index`
		mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
	}

	// mesh_block.pending_update_has_visuals = require_visual;
}

void view_mesh_box(const Box3i box_to_add, VoxelLodTerrainUpdateData::Lod &lod, unsigned int lod_index,
		bool is_full_load_mode, int mesh_to_data_factor, const VoxelData &voxel_data, bool require_visuals,
		bool require_collisions) {
	ZN_PROFILE_SCOPE();

	const Box3i bounds_in_data_blocks = voxel_data.get_bounds().downscaled(voxel_data.get_block_size() << lod_index);

	box_to_add.for_each_cell([&lod, //
									 is_full_load_mode, //
									 mesh_to_data_factor, //
									 &voxel_data, lod_index, //
									 require_visuals, //
									 require_collisions, //
									 bounds_in_data_blocks](Vector3i bpos) {
		VoxelLodTerrainUpdateData::MeshBlockState *mesh_block;
		auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

		if (mesh_block_it == lod.mesh_map_state.map.end()) {
			// RWLockWrite wlock(lod.mesh_map_state.map_lock);
			mesh_block = &insert_new(lod.mesh_map_state.map, bpos);

			// if (is_full_load_mode) {
			// 	// Everything is loaded up-front, so we directly trigger meshing instead of
			// 	// reacting to data chunks being loaded
			// 	schedule_mesh_load(lod.mesh_blocks_pending_update, bpos, *mesh_block, require_visuals);
			// }

		} else {
			mesh_block = &mesh_block_it->second;
		}

		bool first_visuals = false;
		if (require_visuals) {
			first_visuals = mesh_block->mesh_viewers.get() == 0;
			mesh_block->mesh_viewers.add();
		}

		bool first_collision = false;
		if (require_collisions) {
			first_collision = mesh_block->collision_viewers.get() == 0;
			mesh_block->collision_viewers.add();
		}

		if (first_visuals || first_collision) {
			// TODO Optimize: don't schedule again if an update has been sent to the task system with the same options.
			// Currently we only avoid that for requests in the list before they get sent to the task system.
			// This could be a problem if many viewers with increasingly different options are spawned in the same area
			// at consecutive frames.

			// TODO Optimize: don't trigger tasks with options we already scheduled calculations for.
			// For example, in case there is no task in the update list, but the last scheduled ones did compute
			// collision but not visual, if a viewer requests visuals then the scheduled task must only compute
			// visuals. Recomputing collision is unnecessary because the mesh won't have changed in this scenario.
			// (Voxel changes trigger an update of each refcounted option and use a different code path).
			// We would still remesh (and so generate unedited voxel data in some cases) though, and to avoid that we'd
			// need to keep a cache mesh data ourselves. The issue is that Godot is also caching mesh data in ArrayMesh
			// (but for different reasons so it's not reliable), so it would come at a noticeable memory cost.

			if (is_full_load_mode) {
				// Everything is loaded up-front, so we have to directly trigger meshing instead of
				// reacting to data chunks being loaded
				schedule_mesh_load(lod.mesh_blocks_pending_update, bpos, *mesh_block, require_visuals);

			} else {
				// (Re-)Trigger meshing if data is already available.
				// This is needed especially in streaming mode because then there won't be any "data loaded" event to
				// react to if data is already there. Before that, meshes were updated only when a data block was loaded
				// or modified, so changing block size or viewer flags did not make meshes appear. Having two viewer
				// regions meet also caused problems.

				const Box3i data_box = Box3i(bpos * mesh_to_data_factor, Vector3iUtil::create(mesh_to_data_factor))
											   .padded(1)
											   .clipped(bounds_in_data_blocks);

				// If we get an empty box at this point, something is wrong with the caller
				ZN_ASSERT_RETURN(!data_box.is_empty());

				const bool data_available = voxel_data.has_all_blocks_in_area_unbound(data_box, lod_index);

				if (data_available) {
					schedule_mesh_load(lod.mesh_blocks_pending_update, bpos, *mesh_block, require_visuals);
				}
				// Else, we'll react to when data is loaded
			}
		}

#if 0
		// TODO Trigger a mesh update with visuals if that's the first viewer with visuals.
		// Disregard the fact a mesh update is already pending when that happens, unless it was triggered with
		// the same flags.

		// Trigger meshing if data is already available.
		// This is needed because then there won't be any "data loaded" event to react to.
		// Before that, meshes were updated only when a data block was loaded or modified,
		// so changing block size or viewer flags did not make meshes appear. Having two
		// viewer regions meet also caused problems.
		//
		// TODO This tends to suggest that data blocks should be allocated from here
		// instead, however it would couple mesh loading to data loading, forcing to
		// duplicate the data code path in case of viewers that don't need meshes.
		// try_schedule_mesh_update(*block);
		// Alternative: in data diff, put every found block into a list which we'll also
		// run through in `process_loaded_data_blocks_trigger_meshing`?
		//
		if (!is_full_load_mode && (!mesh_block->loaded || first_visuals) &&
				// Is an update already pending?
				mesh_block->state != VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT &&
				mesh_block->state != VoxelLodTerrainUpdateData::MESH_UPDATE_SENT) {
			//
			const Box3i data_box =
					Box3i(bpos * mesh_to_data_factor, Vector3iUtil::create(mesh_to_data_factor)).padded(1);

			// If we get an empty box at this point, something is wrong with the caller
			ZN_ASSERT_RETURN(!data_box.is_empty());

			const bool data_available = voxel_data.has_all_blocks_in_area(data_box, lod_index);

			if (data_available) {
				schedule_mesh_load(lod.mesh_blocks_pending_update, bpos, *mesh_block, first_visuals);
			}
		}
#endif
	});
}

void unview_mesh_box(const Box3i out_of_range_box, VoxelLodTerrainUpdateData::Lod &lod, unsigned int lod_index,
		unsigned int lod_count, VoxelLodTerrainUpdateData::State &state, bool visual_flag, bool collision_flag) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(collision_flag || visual_flag);

	out_of_range_box.for_each_cell([&lod, visual_flag, collision_flag](Vector3i bpos) {
		auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

		if (mesh_block_it != lod.mesh_map_state.map.end()) {
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;

			bool visual_needed;
			if (visual_flag) {
				visual_needed = (mesh_block.mesh_viewers.remove() > 1); // Note, remove() returns the previous count
			} else {
				visual_needed = mesh_block.mesh_viewers.get() > 0;
			}

			bool collision_needed;
			if (collision_flag) {
				collision_needed = (mesh_block.collision_viewers.remove() > 1);
			} else {
				collision_needed = mesh_block.collision_viewers.get() > 0;
			}

			if (collision_needed == false && visual_needed == false) {
				// No viewer needs this mesh anymore

				if (mesh_block.cancellation_token.is_valid()) {
					mesh_block.cancellation_token.cancel();
				}

				if (mesh_block.update_list_index != -1) {
					unordered_remove(lod.mesh_blocks_pending_update, mesh_block.update_list_index);
					mesh_block.update_list_index = -1;
				}

				// TODO What if a viewer causes unload, then another one updates afterward and would have kept the mesh
				// loaded? That will trigger a reload, but if mesh load is fast, what if the main thread unloads the new
				// mesh due to the old momentary unload? Very edge case, but keeping a note in case something weird
				// happens in practice.
				lod.mesh_map_state.map.erase(mesh_block_it);
				lod.mesh_blocks_to_unload.push_back(bpos);

			} else {
				// The block remains but we may unload one of its resources

				if (visual_flag && !visual_needed) {
					// Unload graphics to save memory
					lod.mesh_blocks_to_drop_visual.push_back(bpos);
					// Note, `visuals_loaded` will remain true until they are actually unloaded.
				}

				if (collision_flag && !collision_needed) {
					// Unload colliders to save memory
					lod.mesh_blocks_to_drop_collision.push_back(bpos);
				}
			}
		}
	});

	// Immediately show parent when children are removed.
	// This is a cheap approach as the parent mesh will be available most of the time.
	// However, at high speeds, if loading can't keep up, holes and overlaps will start happening in the
	// opposite direction of movement.
	const unsigned int parent_lod_index = lod_index + 1;
	if (parent_lod_index < lod_count) {
		// Should always work without reaching zero size because non-max LODs are always
		// multiple of 2 due to subdivision rules
		const Box3i parent_box = Box3i(out_of_range_box.pos >> 1, out_of_range_box.size >> 1);

		VoxelLodTerrainUpdateData::Lod &parent_lod = state.lods[parent_lod_index];

		// Show parents when children are removed
		parent_box.for_each_cell([&parent_lod, //
										 &lod, //
										 visual_flag, //
										 collision_flag //
		](Vector3i bpos) {
			auto mesh_it = parent_lod.mesh_map_state.map.find(bpos);

			if (mesh_it != parent_lod.mesh_map_state.map.end()) {
				VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;

				bool activated = false;

				if (visual_flag) {
					if (!mesh_block.visual_active) {
						// Only do merging logic if child chunks were ACTUALLY removed.
						// In multi-viewer scenarios, the clipbox might have moved away from chunks of the
						// child LOD, but another viewer could still reference them, so we should not merge
						// them yet.
						// This check assumes there is always 8 children or no children
						const Vector3i child_bpos0 = bpos << 1;
						auto child_mesh0_it = lod.mesh_map_state.map.find(child_bpos0);

						if (child_mesh0_it == lod.mesh_map_state.map.end() ||
								child_mesh0_it->second.mesh_viewers.get() == 0) {
							mesh_block.visual_active = true;
							parent_lod.mesh_blocks_to_activate_visuals.push_back(bpos);
							activated = true;
						}

						// We know parent_lod_index must be > 0
						// if (parent_lod_index > 0) {
						// This would actually do nothing because children were removed
						// hide_children_recursive(state, parent_lod_index, bpos);
						// }
					}
				}
				if (collision_flag) {
					if (!mesh_block.collision_active) {
						const Vector3i child_bpos0 = bpos << 1;
						auto child_mesh0_it = lod.mesh_map_state.map.find(child_bpos0);

						if (child_mesh0_it == lod.mesh_map_state.map.end() ||
								child_mesh0_it->second.collision_viewers.get() == 0) {
							mesh_block.collision_active = true;
							parent_lod.mesh_blocks_to_activate_collision.push_back(bpos);
							activated = true;
						}
					}
				}

				if (activated) {
					// Voxels of the mesh could have been modified while the mesh was inactive (notably LODs)
					if (mesh_block.state == VoxelLodTerrainUpdateData::MESH_NEED_UPDATE) {
						mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
						mesh_block.update_list_index = lod.mesh_blocks_pending_update.size();
						lod.mesh_blocks_pending_update.push_back(VoxelLodTerrainUpdateData::MeshToUpdate{
								bpos, TaskCancellationToken(), mesh_block.mesh_viewers.get() > 0 });
					}
				}
			}
		});
	}
}

inline bool requires_meshes(const VoxelLodTerrainUpdateData::PairedViewer::State &viewer_state) {
	return viewer_state.requires_collisions || viewer_state.requires_visuals;
}

void process_viewer_mesh_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, int mesh_block_size_po2,
		int lod_count, const Box3i &volume_bounds_in_voxels,
		const VoxelLodTerrainUpdateData::PairedViewer &paired_viewer, bool can_load, bool is_full_load_mode,
		int mesh_to_data_factor, const VoxelData &data) {
	ZN_PROFILE_SCOPE();

#ifdef DEV_ENABLED
	Box3i debug_parent_box;
#endif

	// TODO Optimize: when a viewer doesn't need visuals, we only need to build meshes for collisions up to a certain
	// LOD (collision max LOD property). That would be an optimization for servers, NPCs and player hosts

	// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
	for (int lod_index = lod_count - 1; lod_index >= 0; --lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		const int lod_mesh_block_size_po2 = mesh_block_size_po2 + lod_index;
		const int lod_mesh_block_size = 1 << lod_mesh_block_size_po2;
		// const Vector3i viewer_block_pos_within_lod = math::floor_to_int(p_viewer_pos) >> block_size_po2;

		const Box3i bounds_in_mesh_blocks = volume_bounds_in_voxels.downscaled(lod_mesh_block_size);

		// const Box3i new_mesh_box = get_lod_box_in_chunks(
		// 		viewer_pos_in_lod0_voxels, lod_distance_in_mesh_chunks, mesh_block_size_po2, lod_index)
		// 								   .clipped(bounds_in_mesh_blocks);

		const Box3i &new_mesh_box = paired_viewer.state.mesh_box_per_lod[lod_index];
		const Box3i &prev_mesh_box = paired_viewer.prev_state.mesh_box_per_lod[lod_index];

#ifdef DEV_ENABLED
		if (lod_index + 1 != lod_count) {
			const Box3i debug_parent_box_in_current_lod(debug_parent_box.pos << 1, debug_parent_box.size << 1);
			ZN_ASSERT(debug_parent_box_in_current_lod.contains(new_mesh_box));
		}
		debug_parent_box = new_mesh_box;
#endif

		// const Box3i prev_mesh_box = get_lod_box_in_chunks(
		// 		state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update,
		// 		state.clipbox_streaming.lod_distance_in_mesh_chunks_previous_update, mesh_block_size_po2, lod_index)
		// 									.clipped(bounds_in_mesh_blocks);

		if (!new_mesh_box.intersects(bounds_in_mesh_blocks) && !prev_mesh_box.intersects(bounds_in_mesh_blocks)) {
			// If this box doesn't intersect either now or before, there is no chance a smaller one will
			break;
		}

		if (prev_mesh_box != new_mesh_box) {
			RWLockWrite wlock(lod.mesh_map_state.map_lock);

			// Add meshes entering range
			if (requires_meshes(paired_viewer.state) && can_load) {
				SmallVector<Box3i, 6> new_mesh_boxes;
				new_mesh_box.difference_to_vec(prev_mesh_box, new_mesh_boxes);

				for (const Box3i &box_to_add : new_mesh_boxes) {
					view_mesh_box(box_to_add, lod, lod_index, is_full_load_mode, mesh_to_data_factor, data,
							paired_viewer.state.requires_visuals, paired_viewer.state.requires_collisions);
				}
			}

			// Remove meshes out or range
			if (requires_meshes(paired_viewer.prev_state)) {
				SmallVector<Box3i, 6> old_mesh_boxes;
				prev_mesh_box.difference_to_vec(new_mesh_box, old_mesh_boxes);

				for (const Box3i &out_of_range_box : old_mesh_boxes) {
					unview_mesh_box(out_of_range_box, lod, lod_index, lod_count, state,
							// Use previous state because old boxes were loaded because of them
							paired_viewer.prev_state.requires_visuals, paired_viewer.prev_state.requires_collisions);
				}
			}
		}

		// Handle viewer flags changes at runtime. However I can't think of a use case at the moment, outside of
		// temporary editor stuff. It should be rare, or just never happen.
		// This operates on a DISTINCT set of blocks than the one above.
		// Also, this won't do anything on new viewers that have no previous state, because the previous box will be
		// empty.
		if (!Vector3iUtil::is_empty_size(prev_mesh_box.size)) {
			if (paired_viewer.state.requires_collisions != paired_viewer.prev_state.requires_collisions) {
				const Box3i box = new_mesh_box.clipped(prev_mesh_box);
				if (paired_viewer.state.requires_collisions) {
					// Add refcount to just collisions
					view_mesh_box(box, lod, lod_index, is_full_load_mode, mesh_to_data_factor, data, false, true);
				} else {
					// Remove refcount to just collisions
					unview_mesh_box(box, lod, lod_index, lod_count, state, false, true);
				}
			}

			if (paired_viewer.state.requires_visuals != paired_viewer.prev_state.requires_visuals) {
				const Box3i box = new_mesh_box.clipped(prev_mesh_box);
				if (paired_viewer.state.requires_visuals) {
					view_mesh_box(box, lod, lod_index, is_full_load_mode, mesh_to_data_factor, data, true, false);
				} else {
					unview_mesh_box(box, lod, lod_index, lod_count, state, true, false);
				}
			}
		}

		// {
		// 	ZN_PROFILE_SCOPE_NAMED("Cancel updates");
		// 	// Cancel block updates that are not within the new region
		// 	unordered_remove_if(lod.mesh_blocks_pending_update,
		// 			[new_mesh_box](const VoxelLodTerrainUpdateData::MeshToUpdate &mtu) { //
		// 				return !new_mesh_box.contains(mtu.position);
		// 			});
		// }
	}
}

void process_mesh_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const Box3i bounds_in_voxels, int lod_count,
		bool is_full_load_mode, bool can_load, const VoxelData &data, int data_block_size) {
	ZN_PROFILE_SCOPE();

	const int mesh_block_size_po2 = settings.mesh_block_size_po2;

	// const int lod_distance_in_mesh_chunks = get_lod_distance_in_mesh_chunks(settings.lod_distance, mesh_block_size);

	const int mesh_block_size = 1 << mesh_block_size_po2;
	const int mesh_to_data_factor = mesh_block_size / data_block_size;

	for (const VoxelLodTerrainUpdateData::PairedViewer &paired_viewer : state.clipbox_streaming.paired_viewers) {
		// Only update around viewers that need meshes.
		// Check previous state too in case we have to handle them changing
		if (requires_meshes(paired_viewer.state) || requires_meshes(paired_viewer.prev_state)) {
			process_viewer_mesh_blocks_sliding_box(state, mesh_block_size_po2, lod_count, bounds_in_voxels,
					paired_viewer, can_load, is_full_load_mode, mesh_to_data_factor, data);
		}
	}

	// VoxelLodTerrainUpdateData::ClipboxStreamingState &clipbox_streaming = state.clipbox_streaming;
	// clipbox_streaming.lod_distance_in_mesh_chunks_previous_update = lod_distance_in_mesh_chunks;
}

void process_loaded_data_blocks_trigger_meshing(const VoxelData &data, VoxelLodTerrainUpdateData::State &state,
		const VoxelLodTerrainUpdateData::Settings &settings, const Box3i bounds_in_voxels) {
	ZN_PROFILE_SCOPE();
	// This function should only be used when data streaming is on.
	// When everything is loaded, there is also the assumption that blocks can be generated on the fly, so loading
	// events come in sparsely for only edited areas. So it doesn't make much sense to trigger meshing in reaction to
	// data loading.
	ZN_ASSERT_RETURN(data.is_streaming_enabled());

	const int mesh_block_size_po2 = settings.mesh_block_size_po2;

	VoxelLodTerrainUpdateData::ClipboxStreamingState &clipbox_streaming = state.clipbox_streaming;

	// Get list of data blocks that were loaded since the last update
	static thread_local StdVector<VoxelLodTerrainUpdateData::BlockLocation> tls_loaded_blocks;
	tls_loaded_blocks.clear();
	{
		MutexLock mlock(clipbox_streaming.loaded_data_blocks_mutex);
		append_array(tls_loaded_blocks, clipbox_streaming.loaded_data_blocks);
		clipbox_streaming.loaded_data_blocks.clear();
	}

	// TODO Pool memory
	FixedArray<StdUnorderedSet<Vector3i>, constants::MAX_LOD> checked_mesh_blocks_per_lod;

	const int data_to_mesh_shift = mesh_block_size_po2 - data.get_block_size_po2();

	for (VoxelLodTerrainUpdateData::BlockLocation bloc : tls_loaded_blocks) {
		// ZN_PROFILE_SCOPE_NAMED("Block");
		// Multiple mesh blocks may be interested because of neighbor dependencies.

		// We could group loaded blocks by LOD so we could compute a few things less times?
		const int lod_data_block_size_po2 = data.get_block_size_po2() + bloc.lod;
		const Box3i bounds_in_data_blocks = Box3i( //
				bounds_in_voxels.pos >> lod_data_block_size_po2, //
				bounds_in_voxels.size >> lod_data_block_size_po2);

		const Box3i data_neighboring =
				Box3i(bloc.position - Vector3i(1, 1, 1), Vector3i(3, 3, 3)).clipped(bounds_in_data_blocks);

		StdUnorderedSet<Vector3i> &checked_mesh_blocks = checked_mesh_blocks_per_lod[bloc.lod];
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[bloc.lod];

		const unsigned int lod_index = bloc.lod;

		data_neighboring.for_each_cell([data_to_mesh_shift, &checked_mesh_blocks, &lod, &data, lod_index,
											   &bounds_in_data_blocks](Vector3i data_bpos) {
			// ZN_PROFILE_SCOPE_NAMED("Cell");

			const Vector3i mesh_block_pos = data_bpos >> data_to_mesh_shift;
			if (!checked_mesh_blocks.insert(mesh_block_pos).second) {
				// Already checked
				return;
			}

			// We don't add/remove items from the map here, and only the update task can do that, so no need
			// to lock
			// RWLockRead rlock(lod.mesh_map_state.map_lock);
			auto mesh_it = lod.mesh_map_state.map.find(mesh_block_pos);
			if (mesh_it == lod.mesh_map_state.map.end()) {
				// Not requested
				return;
			}
			VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;
			const VoxelLodTerrainUpdateData::MeshState mesh_state = mesh_block.state;

			// TODO Check if there is more flags to compute with the mesh (collider? rendering?)
			if (mesh_state != VoxelLodTerrainUpdateData::MESH_NEED_UPDATE &&
					mesh_state != VoxelLodTerrainUpdateData::MESH_NEVER_UPDATED) {
				// Already updated or updating
				return;
			}

			bool data_available = true;
			// if (data.is_streaming_enabled()) {
			const Box3i data_box = Box3i((mesh_block_pos << data_to_mesh_shift) - Vector3i(1, 1, 1),
					Vector3iUtil::create((1 << data_to_mesh_shift) + 2))
										   .clipped(bounds_in_data_blocks);
			// TODO Do a single grid query up-front, they will overlap so we do redundant lookups!
			data_available = data.has_all_blocks_in_area_unbound(data_box, lod_index);
			// } else {
			// 	if (!data.is_full_load_completed()) {
			// 		ZN_PRINT_ERROR("This function should not run until full load has completed");
			// 	}
			// }

			if (data_available) {
				schedule_mesh_load(
						lod.mesh_blocks_pending_update, mesh_block_pos, mesh_block, mesh_block.mesh_viewers.get() > 0);
				// We assume data blocks won't unload after this, until data is gathered, because unloading
				// runs before this logic.
			}
		});
	}
}

// void debug_dump_mesh_maps(const VoxelLodTerrainUpdateData::State &state, unsigned int lod_count) {
// 	std::ofstream ofs("ddd_meshmaps.json", std::ios::binary | std::ios::trunc);
// 	ofs << "[";
// 	for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
// 		const VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
// 		if (lod_index > 0) {
// 			ofs << ",";
// 		}
// 		ofs << "[";
// 		for (auto it = lod.mesh_map_state.map.begin(); it != lod.mesh_map_state.map.end(); ++it) {
// 			const Vector3i pos = it->first;
// 			const VoxelLodTerrainUpdateData::MeshBlockState &ms = it->second;
// 			if (it != lod.mesh_map_state.map.begin()) {
// 				ofs << ",";
// 			}
// 			ofs << "[";
// 			ofs << "[";
// 			ofs << pos.x;
// 			ofs << ",";
// 			ofs << pos.y;
// 			ofs << ",";
// 			ofs << pos.z;
// 			ofs << "],";
// 			ofs << static_cast<int>(ms.state);
// 			ofs << ",";
// 			ofs << ms.active;
// 			ofs << ",";
// 			ofs << ms.loaded;
// 			ofs << ",";
// 			ofs << ms.mesh_viewers.get();
// 			ofs << "]";
// 		}
// 		ofs << "]";
// 	}
// 	ofs << "]";
// 	ofs.close();
// }

enum MeshBlockFeatureIndex { //
	MESH_VISUAL = 0,
	MESH_COLLIDER = 1
};

bool is_loaded(const VoxelLodTerrainUpdateData::MeshBlockState &ms, MeshBlockFeatureIndex i) {
	switch (i) {
		case MESH_VISUAL:
			return ms.visual_loaded;
		case MESH_COLLIDER:
			return ms.collision_loaded;
		default:
			ZN_CRASH();
			return false;
	}
}

bool is_active(const VoxelLodTerrainUpdateData::MeshBlockState &ms, MeshBlockFeatureIndex i) {
	switch (i) {
		case MESH_VISUAL:
			return ms.visual_active;
		case MESH_COLLIDER:
			return ms.collision_active;
		default:
			ZN_CRASH();
			return false;
	}
}

void set_active(VoxelLodTerrainUpdateData::MeshBlockState &mesh_block, MeshBlockFeatureIndex feature_index,
		VoxelLodTerrainUpdateData::Lod &lod, const Vector3i bpos) {
	switch (feature_index) {
		case MESH_VISUAL:
			if (!mesh_block.visual_active) {
				mesh_block.visual_active = true;
				lod.mesh_blocks_to_activate_visuals.push_back(bpos);
			}
			break;
		case MESH_COLLIDER:
			if (!mesh_block.collision_active) {
				mesh_block.collision_active = true;
				lod.mesh_blocks_to_activate_collision.push_back(bpos);
			}
			break;
		default:
			ZN_CRASH();
			break;
	}
}

void set_inactive(VoxelLodTerrainUpdateData::MeshBlockState &mesh_block, MeshBlockFeatureIndex feature_index,
		VoxelLodTerrainUpdateData::Lod &lod, const Vector3i bpos) {
	switch (feature_index) {
		case MESH_VISUAL:
			if (mesh_block.visual_active) {
				mesh_block.visual_active = false;
				lod.mesh_blocks_to_deactivate_visuals.push_back(bpos);
			}
			break;
		case MESH_COLLIDER:
			if (mesh_block.collision_active) {
				mesh_block.collision_active = false;
				lod.mesh_blocks_to_deactivate_collision.push_back(bpos);
			}
			break;
		default:
			ZN_CRASH();
			break;
	}
}

// Activates mesh blocks when they are loaded. Activates higher LODs and hides lower LODs when possible.
// This essentially runs octree subdivision logic, but only from a specific node and its descendants.
void update_mesh_block_load(VoxelLodTerrainUpdateData::State &state, Vector3i bpos, unsigned int lod_index,
		unsigned int lod_count, MeshBlockFeatureIndex feature_index) {
	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
	auto mesh_it = lod.mesh_map_state.map.find(bpos);

	if (mesh_it == lod.mesh_map_state.map.end()) {
		return;
	}
	VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;

	if (!is_loaded(mesh_block, feature_index)) {
		return;
	}

	// The mesh is loaded in specified flags

	const unsigned int parent_lod_index = lod_index + 1;
	if (parent_lod_index == lod_count) {
		// Root
		// We don't need to bother about subdivison rules here (no need to check siblings) because there is no parent

		// TODO Don't activate a block if it is already subdivided
		set_active(mesh_block, feature_index, lod, bpos);

		if (lod_index > 0) {
			const unsigned int child_lod_index = lod_index - 1;
			for (unsigned int child_index = 0; child_index < 8; ++child_index) {
				const Vector3i child_bpos = get_child_position(bpos, child_index);
				update_mesh_block_load(state, child_bpos, child_lod_index, lod_count, feature_index);
			}
		}

	} else {
		// Not root
		// We'll have to consider siblings since we can't activate only one at a time, it has to be all or none

		const Vector3i parent_bpos = bpos >> 1;
		VoxelLodTerrainUpdateData::Lod &parent_lod = state.lods[parent_lod_index];

		auto parent_mesh_it = parent_lod.mesh_map_state.map.find(parent_bpos);
		// if (parent_mesh_it == parent_lod.mesh_map_state.map.end()) {
		// 	debug_dump_mesh_maps(state, lod_count);
		// }
		// The parent must exist because sliding boxes contain each other. Maybe in the future that won't always be true
		// if a viewer has special behavior?
		ZN_ASSERT_RETURN_MSG(parent_mesh_it != parent_lod.mesh_map_state.map.end(),
				"Expected parent due to subdivision rules, bug?");

		VoxelLodTerrainUpdateData::MeshBlockState &parent_mesh_block = parent_mesh_it->second;

		if (is_active(parent_mesh_block, feature_index)) {
			bool all_siblings_loaded = true;

			// Test if all siblings are loaded
			// TODO This needs to be optimized. Store a cache in parent?
			for (unsigned int sibling_index = 0; sibling_index < 8; ++sibling_index) {
				const Vector3i sibling_bpos = get_child_position(parent_bpos, sibling_index);
				auto sibling_it = lod.mesh_map_state.map.find(sibling_bpos);
				if (sibling_it == lod.mesh_map_state.map.end()) {
					// Finding this in the mesh map would be weird due to subdivision rules. We don't expect a sibling
					// to be missing, because every mesh block always has 8 children.
					ZN_PRINT_ERROR("Didn't expect missing sibling");
					all_siblings_loaded = false;
					break;
				}
				const VoxelLodTerrainUpdateData::MeshBlockState &sibling = sibling_it->second;
				if (!is_loaded(sibling, feature_index)) {
					all_siblings_loaded = false;
					break;
				}
			}

			if (all_siblings_loaded) {
				// Hide parent
				set_inactive(parent_mesh_block, feature_index, parent_lod, parent_bpos);

				// Show siblings
				for (unsigned int sibling_index = 0; sibling_index < 8; ++sibling_index) {
					const Vector3i sibling_bpos = get_child_position(parent_bpos, sibling_index);
					auto sibling_it = lod.mesh_map_state.map.find(sibling_bpos);
					VoxelLodTerrainUpdateData::MeshBlockState &sibling = sibling_it->second;
					// TODO Optimize: if that sibling itself subdivides, it should not need to be made visible.
					// Maybe make `update_mesh_block_load` return that info so we can avoid scheduling activation?
					set_active(sibling, feature_index, lod, sibling_bpos);

					if (lod_index > 0) {
						const unsigned int child_lod_index = lod_index - 1;
						for (unsigned int child_index = 0; child_index < 8; ++child_index) {
							const Vector3i child_bpos = get_child_position(sibling_bpos, sibling_index);
							update_mesh_block_load(state, child_bpos, child_lod_index, lod_count, feature_index);
						}
					}
				}
			}
		}
	}
}

void process_loaded_mesh_blocks_trigger_visibility_changes(
		VoxelLodTerrainUpdateData::State &state, unsigned int lod_count) {
	ZN_PROFILE_SCOPE();

	VoxelLodTerrainUpdateData::ClipboxStreamingState &clipbox_streaming = state.clipbox_streaming;

	// Get list of mesh blocks that were loaded since the last update
	// TODO Use the same pool buffer as data blocks?
	static thread_local StdVector<VoxelLodTerrainUpdateData::LoadedMeshBlockEvent> tls_loaded_blocks;
	tls_loaded_blocks.clear();
	{
		// If this has contention, we can afford trying to lock and skip if it fails
		MutexLock mlock(clipbox_streaming.loaded_mesh_blocks_mutex);
		append_array(tls_loaded_blocks, clipbox_streaming.loaded_mesh_blocks);
		clipbox_streaming.loaded_mesh_blocks.clear();
	}

	for (const VoxelLodTerrainUpdateData::LoadedMeshBlockEvent event : tls_loaded_blocks) {
		// TODO This isn't optimal. Cost of doing this is doubled if we want both visual and collision.
		if (event.visual) {
			update_mesh_block_load(state, event.position, event.lod_index, lod_count, MESH_VISUAL);
		}
		// TODO We should not need to run this at LODs that have no collision
		if (event.collision) {
			update_mesh_block_load(state, event.position, event.lod_index, lod_count, MESH_COLLIDER);
		}
	}

	{
		uint32_t lods_to_update_transitions = 0;
		for (unsigned int lod_index = 0; lod_index < lod_count; ++lod_index) {
			VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
			// Only update transition masks when visuals change, this is a rendering feature
			if (lod.mesh_blocks_to_activate_visuals.size() > 0 || lod.mesh_blocks_to_deactivate_visuals.size() > 0) {
				lods_to_update_transitions |= (0b111 << lod_index);
			}
		}
		// TODO This is quite slow (see implementation).
		// Maybe there is a way to optimize it with the clipbox logic (updates could be grouped per new/old boxes,
		// however it wouldn't work as-is because mesh updates take time before they actually become visible. Could
		// also update masks incrementally somehow?). The initial reason this streaming system was added was to help
		// with server-side performance. This feature is client-only, so it didn't need to be optimized too at the
		// moment.
		update_transition_masks(state, lods_to_update_transitions, lod_count, true);
	}
}

} // namespace

void process_clipbox_streaming(VoxelLodTerrainUpdateData::State &state, VoxelData &data,
		Span<const std::pair<ViewerID, VoxelEngine::Viewer>> viewers, const Transform3D &volume_transform,
		StdVector<VoxelData::BlockToSave> *data_blocks_to_save,
		StdVector<VoxelLodTerrainUpdateData::BlockToLoad> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, bool can_load, bool can_mesh) {
	ZN_PROFILE_SCOPE();

	const unsigned int lod_count = data.get_lod_count();
	const Box3i bounds_in_voxels = data.get_bounds();
	const unsigned int data_block_size_po2 = data.get_block_size_po2();
	const bool streaming_enabled = data.is_streaming_enabled();
	const bool full_load_completed = data.is_full_load_completed();

	StdVector<unsigned int> unpaired_viewers_to_remove;

	process_viewers(state.clipbox_streaming, settings, lod_count, viewers, volume_transform, bounds_in_voxels,
			data_block_size_po2, can_mesh, unpaired_viewers_to_remove);

	if (streaming_enabled) {
		process_data_blocks_sliding_box(
				state, data, data_blocks_to_save, data_blocks_to_load, settings, lod_count, can_load);
	} else {
		if (full_load_completed == false) {
			// Don't do anything until things are loaded, because we'll trigger meshing directly when mesh blocks get
			// created. If we let this happen before, mesh blocks will get created but we won't have a way to tell when
			// to trigger meshing per block. If we need to do that in the future though, we could diff the "fully
			// loaded" state and iterate all mesh blocks when it becomes true?
			return;
		}
	}

	process_mesh_blocks_sliding_box(
			state, settings, bounds_in_voxels, lod_count, !streaming_enabled, can_load, data, 1 << data_block_size_po2);

	// Removing paired viewers after box diffs because we interpret viewer removal as boxes becoming zero-size, so we
	// need one processing step to handle that before actually removing them
	remove_unpaired_viewers(unpaired_viewers_to_remove, state.clipbox_streaming.paired_viewers);

	if (streaming_enabled) {
		// TODO Have an option to turn off meshing entirely (may be useful on servers if the game doesn't use mesh
		// colliders)
		process_loaded_data_blocks_trigger_meshing(data, state, settings, bounds_in_voxels);
	}

	process_loaded_mesh_blocks_trigger_visibility_changes(state, lod_count);

	// state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update = viewer_pos_in_lod0_voxels;
}

} // namespace zylann::voxel
