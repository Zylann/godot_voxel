#ifndef VOXEL_STREAM_H
#define VOXEL_STREAM_H

#include "../generators/voxel_generator.h"
#include "voxel_block_request.h"
#include <core/resource.h>

// Provides access to a source of paged voxel data, which may load and save.
// This is intented for files, so it may run in a single background thread and gets requests in batches.
// Must be implemented in a thread-safe way.
//
// If you are looking for a more specialized API to generate voxels with more threads, use VoxelGenerator.
//
class VoxelStream : public Resource {
	GDCLASS(VoxelStream, Resource)
public:
	VoxelStream();
	~VoxelStream();

	enum Result {
		// Something went wrong, the request should be aborted
		RESULT_ERROR,
		// The block could not be found in the stream. The requester may fallback on the generator.
		RESULT_BLOCK_NOT_FOUND,
		// The block was found, so the requester won't use the generator.
		RESULT_BLOCK_FOUND,

		_RESULT_COUNT
	};

	// TODO Rename load_block()
	// Queries a block of voxels beginning at the given world-space voxel position and LOD.
	// If you use LOD, the result at a given coordinate must always remain the same regardless of it.
	// In other words, voxels values must solely depend on their coordinates or fixed parameters.
	virtual Result emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);

	// TODO Rename save_block()
	virtual void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod);

	// Note: vector is passed by ref for performance. Don't reorder it.
	virtual void emerge_blocks(Vector<VoxelBlockRequest> &p_blocks, Vector<Result> &out_results);

	// Returns multiple blocks of voxels to the stream.
	// This function is recommended if you save to files, because you can batch their access.
	virtual void immerge_blocks(const Vector<VoxelBlockRequest> &p_blocks);

	// Declares the format expected from this stream
	virtual int get_used_channels_mask() const;

	// Gets which block size this stream will provide, as a power of two.
	// File streams are likely to impose a specific block size,
	// and changing it can be very expensive so the API is usually specific too
	virtual int get_block_size_po2() const;

	// Gets at how many levels of details blocks can be queried.
	virtual int get_lod_count() const;

	// Should generated blocks be saved immediately? If not, they will be saved only when modified.
	void set_save_generator_output(bool enabled);
	bool get_save_generator_output() const;

private:
	static void _bind_methods();

	void _b_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
	void _b_immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod);
	int _b_get_used_channels_mask() const;
	Vector3 _b_get_block_size() const;

	struct Parameters {
		bool save_generator_output = true;
	};

	Parameters _parameters;
	RWLock *_parameters_lock = nullptr;
};

VARIANT_ENUM_CAST(VoxelStream::Result);

#endif // VOXEL_STREAM_H
