#include "mesh_block_task.h"
#include "../util/macros.h"
#include "../util/profiling.h"
#include "voxel_server.h"

namespace zylann::voxel {

// Takes a list of blocks and interprets it as a cube of blocks centered around the area we want to create a mesh from.
// Voxels from central blocks are copied, and part of side blocks are also copied so we get a temporary buffer
// which includes enough neighbors for the mesher to avoid doing bound checks.
static void copy_block_and_neighbors(Span<std::shared_ptr<VoxelBufferInternal>> blocks, VoxelBufferInternal &dst,
		int min_padding, int max_padding, int channels_mask, Ref<VoxelGenerator> generator, int data_block_size,
		uint8_t lod_index, Vector3i mesh_block_pos) {
	VOXEL_PROFILE_SCOPE();

	// Extract wanted channels in a list
	unsigned int channels_count = 0;
	FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels =
			VoxelBufferInternal::mask_to_channels_list(channels_mask, channels_count);

	// Determine size of the cube of blocks
	int edge_size;
	int mesh_block_size_factor;
	switch (blocks.size()) {
		case 3 * 3 * 3:
			edge_size = 3;
			mesh_block_size_factor = 1;
			break;
		case 4 * 4 * 4:
			edge_size = 4;
			mesh_block_size_factor = 2;
			break;
		default:
			ERR_FAIL_MSG("Unsupported block count");
	}

	// Pick anchor block, usually within the central part of the cube (that block must be valid)
	const unsigned int anchor_buffer_index = edge_size * edge_size + edge_size + 1;

	std::shared_ptr<VoxelBufferInternal> &central_buffer = blocks[anchor_buffer_index];
	ERR_FAIL_COND_MSG(central_buffer == nullptr && generator.is_null(), "Central buffer must be valid");
	if (central_buffer != nullptr) {
		ERR_FAIL_COND_MSG(
				Vector3iUtil::all_members_equal(central_buffer->get_size()) == false, "Central buffer must be cubic");
	}
	const int mesh_block_size = data_block_size * mesh_block_size_factor;
	const int padded_mesh_block_size = mesh_block_size + min_padding + max_padding;

	dst.create(padded_mesh_block_size, padded_mesh_block_size, padded_mesh_block_size);

	// TODO Need to provide format
	// for (unsigned int ci = 0; ci < channels.size(); ++ci) {
	// 	dst.set_channel_depth(ci, central_buffer->get_channel_depth(ci));
	// }

	const Vector3i min_pos = -Vector3iUtil::create(min_padding);
	const Vector3i max_pos = Vector3iUtil::create(mesh_block_size + max_padding);

	std::vector<Box3i> boxes_to_generate;
	const Box3i mesh_data_box = Box3i::from_min_max(min_pos, max_pos);
	if (generator.is_valid()) {
		boxes_to_generate.push_back(mesh_data_box);
	}

	// Using ZXY as convention to reconstruct positions with thread locking consistency
	unsigned int block_index = 0;
	for (int z = -1; z < edge_size - 1; ++z) {
		for (int x = -1; x < edge_size - 1; ++x) {
			for (int y = -1; y < edge_size - 1; ++y) {
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

				if (generator.is_valid()) {
					// Subtract edited box from the area to generate
					// TODO This approach allows to batch boxes if necessary,
					// but is it just better to do it anyways for every clipped box?
					VOXEL_PROFILE_SCOPE_NAMED("Box subtract");
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

	if (generator.is_valid()) {
		// Complete data with generated voxels
		VOXEL_PROFILE_SCOPE_NAMED("Generate");
		VoxelBufferInternal generated_voxels;

		const Vector3i origin_in_voxels = mesh_block_pos * (mesh_block_size_factor * data_block_size << lod_index);

		for (unsigned int i = 0; i < boxes_to_generate.size(); ++i) {
			const Box3i &box = boxes_to_generate[i];
			//print_line(String("size={0}").format(varray(box.size.to_vec3())));
			generated_voxels.create(box.size);
			//generated_voxels.set_voxel_f(2.0f, box.size.x / 2, box.size.y / 2, box.size.z / 2,
			//VoxelBufferInternal::CHANNEL_SDF);
			VoxelGenerator::VoxelQueryData q{ generated_voxels, (box.pos << lod_index) + origin_in_voxels, lod_index };
			generator->generate_block(q);

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
	VOXEL_PROFILE_SCOPE();
	CRASH_COND(meshing_dependency == nullptr);

	Ref<VoxelMesher> mesher = meshing_dependency->mesher;
	CRASH_COND(mesher.is_null());
	const unsigned int min_padding = mesher->get_minimum_padding();
	const unsigned int max_padding = mesher->get_maximum_padding();

	// TODO Cache?
	VoxelBufferInternal voxels;
	copy_block_and_neighbors(to_span(blocks, blocks_count), voxels, min_padding, max_padding,
			mesher->get_used_channels_mask(), meshing_dependency->generator, data_block_size, lod, position);

	const VoxelMesher::Input input = { voxels, lod };
	mesher->build(_surfaces_output, input);

	_has_run = true;
}

int MeshBlockTask::get_priority() {
	float closest_viewer_distance_sq;
	const int p = priority_dependency.evaluate(lod, &closest_viewer_distance_sq);
	_too_far = closest_viewer_distance_sq > priority_dependency.drop_distance_squared;
	return p;
}

bool MeshBlockTask::is_cancelled() {
	return !meshing_dependency->valid || _too_far;
}

void MeshBlockTask::apply_result() {
	if (VoxelServer::get_singleton()->is_volume_valid(volume_id)) {
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
			o.lod = lod;
			o.surfaces = std::move(_surfaces_output);

			VoxelServer::VolumeCallbacks callbacks = VoxelServer::get_singleton()->get_volume_callbacks(volume_id);
			ERR_FAIL_COND(callbacks.mesh_output_callback == nullptr);
			ERR_FAIL_COND(callbacks.data == nullptr);
			callbacks.mesh_output_callback(callbacks.data, o);
		}

	} else {
		// This can happen if the user removes the volume while requests are still about to return
		PRINT_VERBOSE("Mesh request response came back but volume wasn't found");
	}
}

} // namespace zylann::voxel
