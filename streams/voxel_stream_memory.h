#ifndef VOXEL_STREAM_MEMORY_H
#define VOXEL_STREAM_MEMORY_H

#include "../constants/voxel_constants.h"
#include "../storage/voxel_buffer.h"
#include "../util/containers/fixed_array.h"
#include "../util/containers/span.h"
#include "../util/containers/std_unordered_map.h"
#include "../util/math/vector3i.h"
#include "../util/memory/memory.h"
#include "../util/thread/mutex.h"
#include "instance_data.h"
#include "voxel_stream.h"

namespace zylann::voxel {

// "fake" stream that just stores copies of the data in memory instead of saving them to the filesystem. May be used for
// testing.
class VoxelStreamMemory : public VoxelStream {
	GDCLASS(VoxelStreamMemory, VoxelStream)
public:
	void load_voxel_blocks(Span<VoxelQueryData> p_blocks) override;
	void save_voxel_blocks(Span<VoxelQueryData> p_blocks) override;
	void load_voxel_block(VoxelQueryData &query_data) override;
	void save_voxel_block(VoxelQueryData &query_data) override;

	bool supports_instance_blocks() const override;
	void load_instance_blocks(Span<InstancesQueryData> out_blocks) override;
	void save_instance_blocks(Span<InstancesQueryData> p_blocks) override;

	bool supports_loading_all_blocks() const override;
	void load_all_blocks(FullLoadingResult &result) override;

	int get_used_channels_mask() const override;

	int get_lod_count() const override;

	void set_artificial_save_latency_usec(int usec);
	int get_artificial_save_latency_usec() const;

private:
	static void _bind_methods();

	struct VoxelChunk {
		VoxelBuffer voxels;
		VoxelChunk() : voxels(VoxelBuffer::ALLOCATOR_POOL) {}
	};

	struct Lod {
		StdUnorderedMap<Vector3i, VoxelChunk> voxel_blocks;
		StdUnorderedMap<Vector3i, InstanceBlockData> instance_blocks;
		Mutex mutex;
	};

	FixedArray<Lod, constants::MAX_LOD> _lods;
	unsigned int _artificial_save_latency_usec = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_STREAM_MEMORY_H
