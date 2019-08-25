#ifndef VOXEL_STREAM_H
#define VOXEL_STREAM_H

#include "../util/zprofiling.h"
#include "../voxel_buffer.h"
#include <core/resource.h>

// Provides access to a source of paged voxel data.
// Must be implemented in a multi-thread-safe way.
class VoxelStream : public Resource {
	GDCLASS(VoxelStream, Resource)
public:
	struct BlockRequest {
		Ref<VoxelBuffer> voxel_buffer;
		Vector3i origin_in_voxels;
		int lod;
	};

	struct Stats {
		int file_openings = 0;
		int time_spent_opening_files = 0;
	};

	VoxelStream();

	virtual void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);
	virtual void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod);

	// Note: vector is passed by ref for performance. Don't reorder it.
	virtual void emerge_blocks(Vector<BlockRequest> &p_blocks);
	virtual void immerge_blocks(Vector<BlockRequest> &p_blocks);

	virtual bool is_thread_safe() const;
	virtual bool is_cloneable() const;

	Stats get_statistics() const;

protected:
	static void _bind_methods();

	void _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
	void _immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod);

	Stats _stats;

	VOXEL_PROFILER_DECLARE;
};

#endif // VOXEL_STREAM_H
