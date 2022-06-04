#ifndef VOXEL_DATA_BLOCK_H
#define VOXEL_DATA_BLOCK_H

#include "../storage/voxel_buffer_internal.h"
#include "../util/ref_count.h"
#include <memory>

namespace zylann::voxel {

// Stores loaded voxel data for a chunk of the volume. Mesh and colliders are stored separately.
class VoxelDataBlock {
public:
	RefCount viewers;

	VoxelDataBlock() {}

	VoxelDataBlock(std::shared_ptr<VoxelBufferInternal> &buffer, unsigned int p_lod_index) :
			_lod_index(p_lod_index), _voxels(buffer) {}

	VoxelDataBlock(VoxelDataBlock &&src) :
			viewers(src.viewers),
			_lod_index(src._lod_index),
			_voxels(std::move(src._voxels)),
			_needs_lodding(src._needs_lodding),
			_modified(src._modified),
			_edited(src._edited) {}

	VoxelDataBlock &operator=(VoxelDataBlock &&src) {
		viewers = src.viewers;
		_lod_index = src._lod_index;
		_voxels = std::move(src._voxels);
		_needs_lodding = src._needs_lodding;
		_modified = src._modified;
		_edited = src._edited;
		return *this;
	}

	inline unsigned int get_lod_index() const {
		return _lod_index;
	}

	VoxelBufferInternal &get_voxels() {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(_voxels != nullptr);
#endif
		return *_voxels;
	}

	const VoxelBufferInternal &get_voxels_const() const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(_voxels != nullptr);
#endif
		return *_voxels;
	}

	std::shared_ptr<VoxelBufferInternal> get_voxels_shared() const {
#ifdef DEBUG_ENABLED
		ZN_ASSERT(_voxels != nullptr);
#endif
		return _voxels;
	}

	void set_voxels(std::shared_ptr<VoxelBufferInternal> &buffer) {
		ZN_ASSERT_RETURN(buffer != nullptr);
		_voxels = buffer;
	}

	void set_modified(bool modified);

	inline bool is_modified() const {
		return _modified;
	}

	void set_needs_lodding(bool need_lodding) {
		_needs_lodding = need_lodding;
	}

	inline bool get_needs_lodding() const {
		return _needs_lodding;
	}

	inline void set_edited(bool edited) {
		_edited = true;
	}

	inline bool is_edited() const {
		return _edited;
	}

private:
	std::shared_ptr<VoxelBufferInternal> _voxels;

	uint8_t _lod_index = 0;

	// The block was edited, which requires its LOD counterparts to be recomputed
	bool _needs_lodding = false;

	// Indicates if this block is different from the time it was loaded (should be saved)
	bool _modified = false;

	// Tells if the block has ever been edited.
	// If `false`, the same data can be obtained by running the generator.
	// Once it becomes `true`, it usually never comes back to `false` unless reverted.
	bool _edited = false;

	// TODO Optimization: design a proper way to implement client-side caching for multiplayer
	//
	// Represents how many times the block was edited.
	// This allows to implement client-side caching in multiplayer.
	//
	// Note: when doing client-side caching, if the server decides to revert a block to generator output,
	// resetting version to 0 might not be a good idea, because if a client had version 1, it could mismatch with
	// the "new version 1" after the next edit. All clients having ever joined the server would have to be aware
	// of the revert before they start getting blocks with the server,
	// or need to be told which version is the "generated" one.
	//uint32_t _version;

	// Tells if it's worth requesting a more precise version of the data.
	// Will be `true` if it's not worth it.
	//bool _max_lod_hint = false;
};

} // namespace zylann::voxel

#endif // VOXEL_DATA_BLOCK_H
