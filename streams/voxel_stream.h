#ifndef VOXEL_STREAM_H
#define VOXEL_STREAM_H

#include "../util/zprofiling.h"
#include "voxel_block_request.h"
#include <core/resource.h>

// Provides access to a source of paged voxel data, which may load and save.
// Must be implemented in a multi-thread-safe way.
// If you are looking for a more specialized API to generate voxels, use VoxelGenerator.
class VoxelStream : public Resource {
	GDCLASS(VoxelStream, Resource)
public:
	struct Stats {
		int file_openings = 0;
		int time_spent_opening_files = 0;
	};

	VoxelStream();

	// TODO Rename load_block()
	// Queries a block of voxels beginning at the given world-space voxel position and LOD.
	// If you use LOD, the result at a given coordinate must always remain the same regardless of it.
	// In other words, voxels values must solely depend on their coordinates or fixed parameters.
	virtual void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);

	// TODO Rename unload_block(), or save_block() ?
	virtual void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod);

	// Note: vector is passed by ref for performance. Don't reorder it.
	virtual void emerge_blocks(Vector<VoxelBlockRequest> &p_blocks);

	// Returns multiple blocks of voxels to the stream.
	// Generators usually don't implement it.
	// This function is recommended if you save to files, because you can batch their access.
	virtual void immerge_blocks(Vector<VoxelBlockRequest> &p_blocks);

	// Declares the format expected from this stream
	virtual int get_used_channels_mask() const;

	// If the stream is thread-safe, the same instance will be used by the streamer across all threads.
	// If the stream is not thread-safe:
	// - If it is cloneable, the streamer will duplicate the instance for each thread.
	// - If it isn't, the streamer will be limited to a single thread.
	virtual bool is_thread_safe() const;
	virtual bool is_cloneable() const;

	Stats get_statistics() const;

	virtual bool has_script() const;

protected:
	static void _bind_methods();

	void _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
	void _immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod);
	int _get_used_channels_mask() const;

	Stats _stats;
};

#endif // VOXEL_STREAM_H
