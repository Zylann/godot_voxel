#include "mesh_block_task.h"
#include "../storage/voxel_data_map.h"
#include "../util/dstack.h"
#include "../util/log.h"
#include "../util/profiling.h"
#include "voxel_server.h"

namespace zylann::voxel {

struct CubicAreaInfo {
	int edge_size; // In data blocks
	int mesh_block_size_factor;
	unsigned int anchor_buffer_index;

	inline bool is_valid() const {
		return edge_size != 0;
	}
};

CubicAreaInfo get_cubic_area_info_from_size(unsigned int size) {
	// Determine size of the cube of blocks
	int edge_size;
	int mesh_block_size_factor;
	switch (size) {
		case 3 * 3 * 3:
			edge_size = 3;
			mesh_block_size_factor = 1;
			break;
		case 4 * 4 * 4:
			edge_size = 4;
			mesh_block_size_factor = 2;
			break;
		default:
			ZN_PRINT_ERROR("Unsupported block count");
			return CubicAreaInfo{ 0, 0, 0 };
	}

	// Pick anchor block, usually within the central part of the cube (that block must be valid)
	const unsigned int anchor_buffer_index = edge_size * edge_size + edge_size + 1;

	return { edge_size, mesh_block_size_factor, anchor_buffer_index };
}

// Takes a list of blocks and interprets it as a cube of blocks centered around the area we want to create a mesh from.
// Voxels from central blocks are copied, and part of side blocks are also copied so we get a temporary buffer
// which includes enough neighbors for the mesher to avoid doing bound checks.
static void copy_block_and_neighbors(Span<std::shared_ptr<VoxelBufferInternal>> blocks, VoxelBufferInternal &dst,
		int min_padding, int max_padding, int channels_mask, Ref<VoxelGenerator> generator,
		const VoxelModifierStack *modifiers, int data_block_size, uint8_t lod_index, Vector3i mesh_block_pos) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	// Extract wanted channels in a list
	unsigned int channels_count = 0;
	FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels =
			VoxelBufferInternal::mask_to_channels_list(channels_mask, channels_count);

	// Determine size of the cube of blocks
	const CubicAreaInfo area_info = get_cubic_area_info_from_size(blocks.size());
	ERR_FAIL_COND(!area_info.is_valid());

	std::shared_ptr<VoxelBufferInternal> &central_buffer = blocks[area_info.anchor_buffer_index];
	ERR_FAIL_COND_MSG(central_buffer == nullptr && generator.is_null(), "Central buffer must be valid");
	if (central_buffer != nullptr) {
		ERR_FAIL_COND_MSG(
				Vector3iUtil::all_members_equal(central_buffer->get_size()) == false, "Central buffer must be cubic");
	}
	const int mesh_block_size = data_block_size * area_info.mesh_block_size_factor;
	const int padded_mesh_block_size = mesh_block_size + min_padding + max_padding;

	dst.create(padded_mesh_block_size, padded_mesh_block_size, padded_mesh_block_size);

	// TODO Need to provide format differently, this won't work in full load mode where areas are generated on the fly
	// for (unsigned int ci = 0; ci < channels.size(); ++ci) {
	// 	dst.set_channel_depth(ci, central_buffer->get_channel_depth(ci));
	// }
	// This is a hack
	for (unsigned int i = 0; i < blocks.size(); ++i) {
		const std::shared_ptr<VoxelBufferInternal> &buffer = blocks[i];
		if (buffer != nullptr) {
			// Initialize channel depths from the first non-null block found
			dst.copy_format(*buffer);
			break;
		}
	}

	const Vector3i min_pos = -Vector3iUtil::create(min_padding);
	const Vector3i max_pos = Vector3iUtil::create(mesh_block_size + max_padding);

	std::vector<Box3i> boxes_to_generate;
	const Box3i mesh_data_box = Box3i::from_min_max(min_pos, max_pos);
	const bool has_generator = generator.is_valid() || modifiers != nullptr;
	if (has_generator) {
		boxes_to_generate.push_back(mesh_data_box);
	}

	// Using ZXY as convention to reconstruct positions with thread locking consistency
	unsigned int block_index = 0;
	for (int z = -1; z < area_info.edge_size - 1; ++z) {
		for (int x = -1; x < area_info.edge_size - 1; ++x) {
			for (int y = -1; y < area_info.edge_size - 1; ++y) {
				const Vector3i offset = data_block_size * Vector3i(x, y, z);
				const std::shared_ptr<VoxelBufferInternal> &src = blocks[block_index];
				++block_index;

				if (src == nullptr) {
					continue;
				}

				const Vector3i src_min = min_pos - offset;
				const Vector3i src_max = max_pos - offset;

				{
					RWLockRead read(src->get_lock());
					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						dst.copy_from(*src, src_min, src_max, Vector3i(), channels[ci]);
					}
				}

				if (has_generator) {
					// Subtract edited box from the area to generate
					// TODO This approach allows to batch boxes if necessary,
					// but is it just better to do it anyways for every clipped box?
					ZN_PROFILE_SCOPE_NAMED("Box subtract");
					const unsigned int count = boxes_to_generate.size();
					const Box3i block_box = Box3i(offset, Vector3iUtil::create(data_block_size)).clipped(mesh_data_box);

					for (unsigned int box_index = 0; box_index < count; ++box_index) {
						Box3i box = boxes_to_generate[box_index];
						box.difference(block_box, boxes_to_generate);
#ifdef DEBUG_ENABLED
						CRASH_COND(box_index >= boxes_to_generate.size());
#endif
						boxes_to_generate[box_index] = boxes_to_generate.back();
						boxes_to_generate.pop_back();
					}
				}
			}
		}
	}

	if (has_generator) {
		// Complete data with generated voxels
		ZN_PROFILE_SCOPE_NAMED("Generate");
		VoxelBufferInternal generated_voxels;

		const Vector3i origin_in_voxels =
				mesh_block_pos * (area_info.mesh_block_size_factor * data_block_size << lod_index);

		for (unsigned int i = 0; i < boxes_to_generate.size(); ++i) {
			const Box3i &box = boxes_to_generate[i];
			//print_line(String("size={0}").format(varray(box.size.to_vec3())));
			generated_voxels.create(box.size);
			//generated_voxels.set_voxel_f(2.0f, box.size.x / 2, box.size.y / 2, box.size.z / 2,
			//VoxelBufferInternal::CHANNEL_SDF);
			VoxelGenerator::VoxelQueryData q{ generated_voxels, (box.pos << lod_index) + origin_in_voxels, lod_index };

			if (generator.is_valid()) {
				generator->generate_block(q);
			}
			if (modifiers != nullptr) {
				modifiers->apply(q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << lod_index));
			}

			for (unsigned int ci = 0; ci < channels_count; ++ci) {
				dst.copy_from(generated_voxels, Vector3i(), generated_voxels.get_size(),
						box.pos + Vector3iUtil::create(min_padding), channels[ci]);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
std::atomic_int g_debug_mesh_tasks_count;
} //namespace

MeshBlockTask::MeshBlockTask() {
	++g_debug_mesh_tasks_count;
}

MeshBlockTask::~MeshBlockTask() {
	--g_debug_mesh_tasks_count;
}

int MeshBlockTask::debug_get_running_count() {
	return g_debug_mesh_tasks_count;
}

void MeshBlockTask::run(zylann::ThreadedTaskContext ctx) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();
	CRASH_COND(meshing_dependency == nullptr);

	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	CRASH_COND(mesher.is_null());
	const unsigned int min_padding = mesher->get_minimum_padding();
	const unsigned int max_padding = mesher->get_maximum_padding();

	const VoxelModifierStack *modifiers = data != nullptr ? &data->modifiers : nullptr;

	VoxelBufferInternal voxels;
	copy_block_and_neighbors(to_span(blocks, blocks_count), voxels, min_padding, max_padding,
			mesher->get_used_channels_mask(), meshing_dependency->generator, modifiers, data_block_size, lod_index,
			position);

	// Could cache generator data from here if it was safe to write into the map
	/*if (data != nullptr && cache_generated_blocks) {
		const CubicAreaInfo area_info = get_cubic_area_info_from_size(blocks.size());
		ERR_FAIL_COND(!area_info.is_valid());

		VoxelDataLodMap::Lod &lod = data->lods[lod_index];

		// Note, this box does not include neighbors!
		const Vector3i min_bpos = position * area_info.mesh_block_size_factor;
		const Vector3i max_bpos = min_bpos + Vector3iUtil::create(area_info.edge_size - 2);

		Vector3i bpos;
		for (bpos.z = min_bpos.z; bpos.z < max_bpos.z; ++bpos.z) {
			for (bpos.x = min_bpos.x; bpos.x < max_bpos.x; ++bpos.x) {
				for (bpos.y = min_bpos.y; bpos.y < max_bpos.y; ++bpos.y) {
					// {
					// 	RWLockRead rlock(lod.map_lock);
					// 	VoxelDataBlock *block = lod.map.get_block(bpos);
					// 	if (block != nullptr && (block->is_edited() || block->is_modified())) {
					// 		continue;
					// 	}
					// }
					std::shared_ptr<VoxelBufferInternal> &cache_buffer = make_shared_instance<VoxelBufferInternal>();
					cache_buffer->copy_format(voxels);
					const Vector3i min_src_pos =
							(bpos - min_bpos) * data_block_size + Vector3iUtil::create(min_padding);
					cache_buffer->copy_from(voxels, min_src_pos, min_src_pos + cache_buffer->get_size(), Vector3i());
					// TODO Where to put voxels? Can't safely write to data at the moment.
				}
			}
		}
	}*/

	const Vector3i origin_in_voxels = position * (int(data_block_size) << lod_index);

	const VoxelMesher::Input input = { voxels, meshing_dependency->generator.ptr(), data.get(), origin_in_voxels,
		lod_index, collision_hint, lod_hint };
	mesher->build(_surfaces_output, input);

	_has_run = true;
}

int MeshBlockTask::get_priority() {
	float closest_viewer_distance_sq;
	const int p = priority_dependency.evaluate(lod_index, &closest_viewer_distance_sq);
	_too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool MeshBlockTask::is_cancelled() {
	return !meshing_dependency->valid || _too_far;
}

void MeshBlockTask::apply_result() {
	if (VoxelServer::get_singleton().is_volume_valid(volume_id)) {
		// The request response must match the dependency it would have been requested with.
		// If it doesn't match, we are no longer interested in the result.
		// It is assumed that if a dependency is changed, a new copy of it is made and the old one is marked invalid.
		if (meshing_dependency->valid) {
			VoxelServer::BlockMeshOutput o;
			// TODO Check for invalidation due to property changes

			if (_has_run) {
				o.type = VoxelServer::BlockMeshOutput::TYPE_MESHED;
			} else {
				o.type = VoxelServer::BlockMeshOutput::TYPE_DROPPED;
			}

			o.position = position;
			o.lod = lod_index;
			o.surfaces = std::move(_surfaces_output);

			VoxelServer::VolumeCallbacks callbacks = VoxelServer::get_singleton().get_volume_callbacks(volume_id);
			ERR_FAIL_COND(callbacks.mesh_output_callback == nullptr);
			ERR_FAIL_COND(callbacks.data == nullptr);
			callbacks.mesh_output_callback(callbacks.data, o);
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		ZN_PRINT_VERBOSE("Mesh request response came back but volume wasn't found");
	}
}

} // namespace zylann::voxel
