#ifndef VOXEL_STREAM_H
#define VOXEL_STREAM_H

#include "instance_data.h"
#include "voxel_block_request.h"
#include <core/resource.h>

// Provides access to a source of paged voxel data, which may load and save.
// This is intented for files, so it may run in a single background thread and gets requests in batches.
// Must be implemented in a thread-safe way.
//
// Functions currently don't enforce querying blocks of the same size, however it is required for every stream to
// support querying blocks the size of the declared block size, at positions matching their origins.
// This might be restricted in the future, because there has been no compelling use case for that.
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

	// TODO Deprecate
	// Queries a block of voxels beginning at the given world-space voxel position and LOD.
	// If you use LOD, the result at a given coordinate must always remain the same regardless of it.
	// In other words, voxels values must solely depend on their coordinates or fixed parameters.
	virtual Result emerge_block(VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod);

	// TODO Deprecate
	virtual void immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod);

	// TODO Rename load_voxel_blocks
	// Note: Don't modify the order of `p_blocks`.
	virtual void emerge_blocks(Span<VoxelBlockRequest> p_blocks, Vector<Result> &out_results);

	// TODO Rename save_voxel_blocks
	// Returns multiple blocks of voxels to the stream.
	// This function is recommended if you save to files, because you can batch their access.
	virtual void immerge_blocks(Span<VoxelBlockRequest> p_blocks);

	virtual bool supports_instance_blocks() const;

	virtual void load_instance_blocks(
			Span<VoxelStreamInstanceDataRequest> out_blocks, Span<Result> out_results);

	virtual void save_instance_blocks(Span<VoxelStreamInstanceDataRequest> p_blocks);

	// Tells which channels can be found in this stream.
	// The simplest implementation is to return them all.
	// One reason to specify which channels are available is to help the editor detect configuration issues,
	// and to avoid saving some of the channels if only specific ones are meant to be saved.
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

	Result _b_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
	void _b_immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod);
	int _b_get_used_channels_mask() const;
	Vector3 _b_get_block_size() const;

	struct Parameters {
		bool save_generator_output = false;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

VARIANT_ENUM_CAST(VoxelStream::Result);

#endif // VOXEL_STREAM_H
