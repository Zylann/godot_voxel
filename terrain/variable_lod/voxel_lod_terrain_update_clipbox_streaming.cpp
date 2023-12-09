#include "voxel_lod_terrain_update_clipbox_streaming.h"
#include "../../util/math/conv.h"
#include "../../util/profiling.h"

// #include <fstream>

namespace zylann::voxel {

// Note:
// This streaming method allows every LOD to load in parallel, even before meshes are ready. That means if a data block
// is loaded somewhere and gets edited, there is no guarantee its parent LODs are loaded! It can be made more likely,
// but not guaranteed.
// Hopefully however, this should not be a problem if LODs are just cosmetic representations of
// LOD0. If an edit happens at LOD0 and sibling chunks are present, they can be used to produce the parent LOD.
// Alternatively, LOD updates could wait. In worst case, parent LODs will not update.

namespace {

Box3i get_lod_box_in_chunks(
		Vector3i viewer_pos_in_lod0_voxels, int lod_distance_in_chunks, int chunk_size_po2, int lod_index) {
	// If we only divide positions, the resulting boxes will be biased towards certain axes, causing more details to
	// appear in certain directions on average. To counteract this, we introduce another bias so boxes cover the same
	// area around the viewer, on average.
	// This can be visualized in a graphing calculator with the following formulas:
	// 1) y = floor(x)
	// 2) y = floor(x / 2) * 2
	// 3) y = floor((x + 0.5) / 2) * 2
	// Formula 2) is biased towards positive values.
	// Formula 3) is tweaked to eliminate the bias.
	//
	// I found this by trial and error, so maybe there is an even better solution to this

	// TODO This only works well if lod_distance_in_chunks is even. Any way to fix this?
	// If it is odd, LODs will not align such that each cell matches 8 cells of the child LOD.
	if ((lod_distance_in_chunks & 1) != 0) {
		ZN_PRINT_ERROR("Odd lod distance chunks is unsupported");
	}
	ZN_ASSERT_RETURN_V(lod_distance_in_chunks >= 0, Box3i());
	ZN_ASSERT_RETURN_V(lod_distance_in_chunks < 100, Box3i());

	const int bias = ((1 << lod_index) - 1) << chunk_size_po2;
	const Vector3i vposi2 = viewer_pos_in_lod0_voxels + Vector3i(bias, bias, bias);
	const Vector3i cpos = (vposi2 >> (chunk_size_po2 + lod_index + 1)) * 2;
	const Box3i lod_box = Box3i(cpos, Vector3i(0, 0, 0)).padded(lod_distance_in_chunks);
	return lod_box;
}

bool add_loading_block(VoxelLodTerrainUpdateData::Lod &lod, Vector3i position) {
	auto it = lod.loading_blocks.find(position);

	if (it == lod.loading_blocks.end()) {
		// First viewer to request it
		VoxelLodTerrainUpdateData::LoadingDataBlock new_loading_block;
		new_loading_block.viewers.add();

		lod.loading_blocks.insert({ position, new_loading_block });

		return true;

	} else {
		it->second.viewers.add();
	}

	return false;
}

inline int round_to_next_even(int x) {
	return x + (x & 1);
}

inline int get_lod_distance_in_mesh_chunks(float lod_distance_in_voxels, int mesh_block_size) {
	return math::max(
			// Must be even, otherwise LOD split/merge logic cannot work
			round_to_next_even(static_cast<int>(Math::ceil(lod_distance_in_voxels)) / mesh_block_size), 2);
}

void process_data_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, VoxelData &data,
		Vector3i viewer_pos_in_lod0_voxels, std::vector<VoxelData::BlockToSave> &blocks_to_save, bool can_save,
		// TODO We should be able to work in BOXES to load, it can help compressing network messages
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, int lod_count, bool can_load) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN_MSG(data.is_streaming_enabled(), "This function is not meant to run in full load mode");

	const int data_block_size = data.get_block_size();
	const int data_block_size_po2 = data.get_block_size_po2();
	const Box3i bounds_in_voxels = data.get_bounds();

	const int mesh_block_size = 1 << settings.mesh_block_size_po2;
	const int mesh_to_data_factor = mesh_block_size / data_block_size;

	const int lod_distance_in_mesh_chunks = get_lod_distance_in_mesh_chunks(settings.lod_distance, mesh_block_size);

	// Data chunks are driven by mesh chunks, because mesh needs data
	const int lod_distance_in_data_chunks = lod_distance_in_mesh_chunks * mesh_to_data_factor
			// To account for the fact meshes need neighbor data chunks
			+ 1;

	static thread_local std::vector<Vector3i> tls_missing_blocks;
	static thread_local std::vector<Vector3i> tls_found_blocks_positions;

#ifdef DEV_ENABLED
	Box3i debug_parent_box;
#endif

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

		const Box3i new_data_box = get_lod_box_in_chunks(
				viewer_pos_in_lod0_voxels, lod_distance_in_data_chunks, data_block_size_po2, lod_index)
										   .clipped(bounds_in_data_blocks);

#ifdef DEV_ENABLED
		if (lod_index + 1 != lod_count) {
			const Box3i debug_parent_box_in_current_lod(debug_parent_box.pos << 1, debug_parent_box.size << 1);
			ZN_ASSERT(debug_parent_box_in_current_lod.contains(new_data_box));
		}
		debug_parent_box = new_data_box;
#endif

		const Box3i prev_data_box = get_lod_box_in_chunks(
				state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update,
				state.clipbox_streaming.lod_distance_in_data_chunks_previous_update, data_block_size_po2, lod_index)
											.clipped(bounds_in_data_blocks);

		if (!new_data_box.intersects(bounds_in_data_blocks) && !prev_data_box.intersects(bounds_in_data_blocks)) {
			// If this box doesn't intersect either now or before, there is no chance a smaller one will
			break;
		}

		if (prev_data_box != new_data_box) {
			// Detect blocks to load.
			if (can_load) {
				tls_missing_blocks.clear();

				new_data_box.difference(prev_data_box, [&data, lod_index, &data_blocks_to_load](Box3i box_to_load) {
					data.view_area(box_to_load, lod_index, &tls_missing_blocks, nullptr, nullptr);
				});

				for (const Vector3i bpos : tls_missing_blocks) {
					if (add_loading_block(lod, bpos)) {
						data_blocks_to_load.push_back(
								VoxelLodTerrainUpdateData::BlockLocation{ bpos, static_cast<uint8_t>(lod_index) });
					}
				}
			}

			// Detect blocks to unload
			{
				tls_missing_blocks.clear();
				tls_found_blocks_positions.clear();

				prev_data_box.difference(new_data_box, [&data, &blocks_to_save, lod_index](Box3i box_to_remove) {
					data.unview_area(box_to_remove, lod_index, &tls_found_blocks_positions, &tls_missing_blocks,
							&blocks_to_save);
				});

				// Remove loading blocks (those were loaded and had their refcount reach zero)
				for (const Vector3i bpos : tls_found_blocks_positions) {
					// emit_data_block_unloaded(bpos);

					// TODO If they were loaded, why would they be in loading blocks?
					// Maybe to make sure they are not in here regardless
					lod.loading_blocks.erase(bpos);
				}

				// Remove refcount from loading blocks, and cancel loading if it reaches zero
				for (const Vector3i bpos : tls_missing_blocks) {
					auto loading_block_it = lod.loading_blocks.find(bpos);
					if (loading_block_it == lod.loading_blocks.end()) {
						ZN_PRINT_VERBOSE("Request to unview a loading block that was never requested");
						// Not expected, but fine I guess
						return;
					}

					VoxelLodTerrainUpdateData::LoadingDataBlock &loading_block = loading_block_it->second;
					loading_block.viewers.remove();

					if (loading_block.viewers.get() == 0) {
						// No longer want to load it, no data box contains it
						lod.loading_blocks.erase(loading_block_it);

						VoxelLodTerrainUpdateData::BlockLocation bloc{ bpos, static_cast<uint8_t>(lod_index) };
						for (size_t i = 0; i < data_blocks_to_load.size(); ++i) {
							if (data_blocks_to_load[i] == bloc) {
								data_blocks_to_load[i] = data_blocks_to_load.back();
								data_blocks_to_load.pop_back();
								break;
							}
						}
					}
				}
			}
		}

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

			unordered_remove_if(lod.mesh_blocks_pending_update, [&lod, mesh_box](Vector3i bpos) {
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

	} // for each lod

	state.clipbox_streaming.lod_distance_in_data_chunks_previous_update = lod_distance_in_data_chunks;
}

// TODO Copypasta from octree streaming file
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

inline Vector3i get_child_position_from_first_sibling(Vector3i first_sibling_position, unsigned int child_index) {
	return Vector3i( //
			first_sibling_position.x + (child_index & 1), //
			first_sibling_position.y + ((child_index & 2) >> 1), //
			first_sibling_position.z + ((child_index & 4) >> 2));
}

inline Vector3i get_child_position(Vector3i parent_position, unsigned int child_index) {
	return get_child_position_from_first_sibling(parent_position * 2, child_index);
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

void process_mesh_blocks_sliding_box(VoxelLodTerrainUpdateData::State &state, Vector3i viewer_pos_in_lod0_voxels,
		const VoxelLodTerrainUpdateData::Settings &settings, const Box3i bounds_in_voxels, int lod_count,
		bool is_full_load_mode, bool can_load) {
	ZN_PROFILE_SCOPE();

	const int mesh_block_size_po2 = settings.mesh_block_size_po2;
	const int mesh_block_size = 1 << mesh_block_size_po2;

	const int lod_distance_in_mesh_chunks = get_lod_distance_in_mesh_chunks(settings.lod_distance, mesh_block_size);

#ifdef DEV_ENABLED
	Box3i debug_parent_box;
#endif

	// Iterating from big to small LOD so we can exit earlier if bounds don't intersect.
	for (int lod_index = lod_count - 1; lod_index >= 0; --lod_index) {
		ZN_PROFILE_SCOPE();
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];

		const int lod_mesh_block_size_po2 = mesh_block_size_po2 + lod_index;
		const int lod_mesh_block_size = 1 << lod_mesh_block_size_po2;
		// const Vector3i viewer_block_pos_within_lod = math::floor_to_int(p_viewer_pos) >> block_size_po2;

		const Box3i bounds_in_mesh_blocks = bounds_in_voxels.downscaled(lod_mesh_block_size);

		const Box3i new_mesh_box = get_lod_box_in_chunks(
				viewer_pos_in_lod0_voxels, lod_distance_in_mesh_chunks, mesh_block_size_po2, lod_index)
										   .clipped(bounds_in_mesh_blocks);

#ifdef DEV_ENABLED
		if (lod_index + 1 != lod_count) {
			const Box3i debug_parent_box_in_current_lod(debug_parent_box.pos << 1, debug_parent_box.size << 1);
			ZN_ASSERT(debug_parent_box_in_current_lod.contains(new_mesh_box));
		}
		debug_parent_box = new_mesh_box;
#endif

		const Box3i prev_mesh_box = get_lod_box_in_chunks(
				state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update,
				state.clipbox_streaming.lod_distance_in_mesh_chunks_previous_update, mesh_block_size_po2, lod_index)
											.clipped(bounds_in_mesh_blocks);

		if (!new_mesh_box.intersects(bounds_in_mesh_blocks) && !prev_mesh_box.intersects(bounds_in_mesh_blocks)) {
			// If this box doesn't intersect either now or before, there is no chance a smaller one will
			break;
		}

		if (prev_mesh_box != new_mesh_box) {
			RWLockWrite wlock(lod.mesh_map_state.map_lock);

			// Add meshes entering range
			if (can_load) {
				new_mesh_box.difference(prev_mesh_box, [&lod, lod_index, is_full_load_mode](Box3i box_to_add) {
					box_to_add.for_each_cell([&lod, is_full_load_mode](Vector3i bpos) {
						VoxelLodTerrainUpdateData::MeshBlockState *mesh_block;
						auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

						if (mesh_block_it == lod.mesh_map_state.map.end()) {
							// RWLockWrite wlock(lod.mesh_map_state.map_lock);
							mesh_block = &insert_new(lod.mesh_map_state.map, bpos);

							if (is_full_load_mode) {
								// Everything is loaded up-front, so we directly trigger meshing instead of reacting to
								// data chunks being loaded
								lod.mesh_blocks_pending_update.push_back(bpos);
								mesh_block->state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
							}

						} else {
							mesh_block = &mesh_block_it->second;
						}

						// TODO Viewer options
						mesh_block->mesh_viewers.add();
						mesh_block->collision_viewers.add();
					});
				});
			}

			// Remove meshes out or range
			prev_mesh_box.difference(new_mesh_box, [&lod, lod_index, lod_count, &state](Box3i out_of_range_box) {
				out_of_range_box.for_each_cell([&lod](Vector3i bpos) {
					auto mesh_block_it = lod.mesh_map_state.map.find(bpos);

					if (mesh_block_it != lod.mesh_map_state.map.end()) {
						VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_block_it->second;

						mesh_block.mesh_viewers.remove();
						mesh_block.collision_viewers.remove();

						if (mesh_block.mesh_viewers.get() == 0 && mesh_block.collision_viewers.get() == 0) {
							lod.mesh_map_state.map.erase(bpos);
							lod.mesh_blocks_to_unload.push_back(bpos);
						}
					}
				});

				// Immediately show parent when children are removed.
				// This is a cheap approach as the parent mesh will be available most of the time.
				// However, at high speeds, if loading can't keep up, holes and overlaps will start happening in the
				// opposite direction of movement.
				const int parent_lod_index = lod_index + 1;
				if (parent_lod_index < lod_count) {
					// Should always work without reaching zero size because non-max LODs are always
					// multiple of 2 due to subdivision rules
					const Box3i parent_box = Box3i(out_of_range_box.pos >> 1, out_of_range_box.size >> 1);

					VoxelLodTerrainUpdateData::Lod &parent_lod = state.lods[parent_lod_index];

					// Show parents when children are removed
					parent_box.for_each_cell([&parent_lod, parent_lod_index, &state](Vector3i bpos) {
						auto mesh_it = parent_lod.mesh_map_state.map.find(bpos);

						if (mesh_it != parent_lod.mesh_map_state.map.end()) {
							VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;

							if (!mesh_block.active) {
								mesh_block.active = true;
								parent_lod.mesh_blocks_to_activate.push_back(bpos);

								// We know parent_lod_index must be > 0
								// if (parent_lod_index > 0) {
								// This would actually do nothing because children were removed
								// hide_children_recursive(state, parent_lod_index, bpos);
								// }
							}
						}
					});
				}
			});
		}

		{
			ZN_PROFILE_SCOPE_NAMED("Cancel updates");
			// Cancel block updates that are not within the new region
			unordered_remove_if(lod.mesh_blocks_pending_update, [new_mesh_box](Vector3i bpos) { //
				return !new_mesh_box.contains(bpos);
			});
		}
	}

	VoxelLodTerrainUpdateData::ClipboxStreamingState &clipbox_streaming = state.clipbox_streaming;
	clipbox_streaming.lod_distance_in_mesh_chunks_previous_update = lod_distance_in_mesh_chunks;
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
	static thread_local std::vector<VoxelLodTerrainUpdateData::BlockLocation> tls_loaded_blocks;
	tls_loaded_blocks.clear();
	{
		MutexLock mlock(clipbox_streaming.loaded_data_blocks_mutex);
		append_array(tls_loaded_blocks, clipbox_streaming.loaded_data_blocks);
		clipbox_streaming.loaded_data_blocks.clear();
	}

	// TODO Pool memory
	FixedArray<std::unordered_set<Vector3i>, constants::MAX_LOD> checked_mesh_blocks_per_lod;

	const int data_to_mesh_shift = mesh_block_size_po2 - data.get_block_size_po2();

	for (VoxelLodTerrainUpdateData::BlockLocation bloc : tls_loaded_blocks) {
		// Multiple mesh blocks may be interested because of neighbor dependencies.

		// We could group loaded blocks by LOD so we could compute a few things less times?
		const int lod_data_block_size_po2 = data.get_block_size_po2() + bloc.lod;
		const Box3i bounds_in_data_blocks = Box3i( //
				bounds_in_voxels.pos >> lod_data_block_size_po2, //
				bounds_in_voxels.size >> lod_data_block_size_po2);

		const Box3i data_neighboring =
				Box3i(bloc.position - Vector3i(1, 1, 1), Vector3i(3, 3, 3)).clipped(bounds_in_data_blocks);

		std::unordered_set<Vector3i> &checked_mesh_blocks = checked_mesh_blocks_per_lod[bloc.lod];
		VoxelLodTerrainUpdateData::Lod &lod = state.lods[bloc.lod];

		const unsigned int lod_index = bloc.lod;

		data_neighboring.for_each_cell([data_to_mesh_shift, &checked_mesh_blocks, &lod, &data, lod_index,
											   &bounds_in_data_blocks](Vector3i data_bpos) {
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
			data_available = data.has_all_blocks_in_area(data_box, lod_index);
			// } else {
			// 	if (!data.is_full_load_completed()) {
			// 		ZN_PRINT_ERROR("This function should not run until full load has completed");
			// 	}
			// }

			if (data_available) {
				lod.mesh_blocks_pending_update.push_back(mesh_block_pos);
				mesh_block.state = VoxelLodTerrainUpdateData::MESH_UPDATE_NOT_SENT;
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

// Activates mesh blocks when loaded. Activates higher LODs and hides lower LODs when possible.
// This essentially runs octree subdivision logic, but only from a specific node and its descendants.
void update_mesh_block_load(
		VoxelLodTerrainUpdateData::State &state, Vector3i bpos, unsigned int lod_index, unsigned int lod_count) {
	VoxelLodTerrainUpdateData::Lod &lod = state.lods[lod_index];
	auto mesh_it = lod.mesh_map_state.map.find(bpos);

	if (mesh_it == lod.mesh_map_state.map.end()) {
		return;
	}
	VoxelLodTerrainUpdateData::MeshBlockState &mesh_block = mesh_it->second;
	if (!mesh_block.loaded) {
		return;
	}

	// The mesh is loaded

	const unsigned int parent_lod_index = lod_index + 1;
	if (parent_lod_index == lod_count) {
		// Root
		// We don't need to bother about subdivison rules here (no need to check siblings) because there is no parent

		if (!mesh_block.active) {
			mesh_block.active = true;
			lod.mesh_blocks_to_activate.push_back(bpos);
		}

		if (lod_index > 0) {
			const unsigned int child_lod_index = lod_index - 1;
			for (unsigned int child_index = 0; child_index < 8; ++child_index) {
				const Vector3i child_bpos = get_child_position(bpos, child_index);
				update_mesh_block_load(state, child_bpos, child_lod_index, lod_count);
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

		if (parent_mesh_block.active) {
			bool all_siblings_loaded = true;

			// TODO This needs to be optimized
			for (unsigned int sibling_index = 0; sibling_index < 8; ++sibling_index) {
				const Vector3i sibling_bpos = get_child_position(parent_bpos, sibling_index);
				auto sibling_it = lod.mesh_map_state.map.find(sibling_bpos);
				if (sibling_it == lod.mesh_map_state.map.end()) {
					// Finding this would be weird due to subdivision rules. We don't expect a sibling to be missing,
					// because every mesh block always has 8 children.
					ZN_PRINT_ERROR("Didn't expect missing sibling");
					all_siblings_loaded = false;
					break;
				}
				const VoxelLodTerrainUpdateData::MeshBlockState &sibling = sibling_it->second;
				if (!sibling.loaded) {
					all_siblings_loaded = false;
					break;
				}
			}

			if (all_siblings_loaded) {
				// Hide parent
				parent_mesh_block.active = false;
				parent_lod.mesh_blocks_to_deactivate.push_back(parent_bpos);

				// Show siblings
				for (unsigned int sibling_index = 0; sibling_index < 8; ++sibling_index) {
					const Vector3i sibling_bpos = get_child_position(parent_bpos, sibling_index);
					auto sibling_it = lod.mesh_map_state.map.find(sibling_bpos);
					VoxelLodTerrainUpdateData::MeshBlockState &sibling = sibling_it->second;
					sibling.active = true;
					lod.mesh_blocks_to_activate.push_back(sibling_bpos);

					if (lod_index > 0) {
						const unsigned int child_lod_index = lod_index - 1;
						for (unsigned int child_index = 0; child_index < 8; ++child_index) {
							const Vector3i child_bpos = get_child_position(sibling_bpos, sibling_index);
							update_mesh_block_load(state, child_bpos, child_lod_index, lod_count);
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
	static thread_local std::vector<VoxelLodTerrainUpdateData::BlockLocation> tls_loaded_blocks;
	tls_loaded_blocks.clear();
	{
		// If this has contention, we can afford trying to lock and skip if it fails
		MutexLock mlock(clipbox_streaming.loaded_mesh_blocks_mutex);
		append_array(tls_loaded_blocks, clipbox_streaming.loaded_mesh_blocks);
		clipbox_streaming.loaded_mesh_blocks.clear();
	}

	for (const VoxelLodTerrainUpdateData::BlockLocation bloc : tls_loaded_blocks) {
		update_mesh_block_load(state, bloc.position, bloc.lod, lod_count);
	}
}

} // namespace

void process_clipbox_streaming(VoxelLodTerrainUpdateData::State &state, VoxelData &data, Vector3 viewer_pos,
		std::vector<VoxelData::BlockToSave> &data_blocks_to_save,
		std::vector<VoxelLodTerrainUpdateData::BlockLocation> &data_blocks_to_load,
		const VoxelLodTerrainUpdateData::Settings &settings, Ref<VoxelStream> stream, bool can_load) {
	ZN_PROFILE_SCOPE();

	const Vector3i viewer_pos_in_lod0_voxels = to_vec3i(viewer_pos);
	const unsigned int lod_count = data.get_lod_count();
	const Box3i bounds_in_voxels = data.get_bounds();

	if (data.is_streaming_enabled()) {
		process_data_blocks_sliding_box(state, data, viewer_pos_in_lod0_voxels, data_blocks_to_save, stream.is_valid(),
				data_blocks_to_load, settings, lod_count, can_load);
	} else {
		if (data.is_full_load_completed() == false) {
			// Don't do anything until things are loaded, because we'll trigger meshing directly when mesh blocks get
			// created. If we let this happen before, mesh blocks will get created but we won't have a way to tell when
			// to trigger meshing per block. If we need to do that in the future though, we could diff the "fully
			// loaded" state and iterate all mesh blocks when it becomes true?
			return;
		}
	}

	process_mesh_blocks_sliding_box(state, viewer_pos_in_lod0_voxels, settings, bounds_in_voxels, lod_count,
			!data.is_streaming_enabled(), can_load);

	if (data.is_streaming_enabled()) {
		process_loaded_data_blocks_trigger_meshing(data, state, settings, bounds_in_voxels);
	}

	process_loaded_mesh_blocks_trigger_visibility_changes(state, lod_count);

	// TODO Update transition masks (client side only)

	state.clipbox_streaming.viewer_pos_in_lod0_voxels_previous_update = viewer_pos_in_lod0_voxels;
}

} // namespace zylann::voxel
