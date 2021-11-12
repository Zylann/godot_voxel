#ifndef VOXEL_DATA_BLOCK_H
#define VOXEL_DATA_BLOCK_H

#include <memory>
#include "../storage/voxel_buffer_internal.h"
#include "../util/macros.h"
#include "voxel_ref_count.h"

// Stores loaded voxel data for a chunk of the volume. Mesh and colliders are stored separately.
class VoxelDataBlock {
public:
	const Vector3i position;
	const unsigned int lod_index = 0;
	VoxelRefCount viewers;

	static VoxelDataBlock *create(Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer, unsigned int size,
			unsigned int p_lod_index) {
		const int bs = size;
		ERR_FAIL_COND_V(buffer == nullptr, nullptr);
		ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), nullptr);
		return memnew(VoxelDataBlock(bpos, buffer, p_lod_index));
	}

	VoxelBufferInternal &get_voxels() {
#ifdef DEBUG_ENABLED
		CRASH_COND(_voxels == nullptr);
#endif
		return *_voxels;
	}

	const VoxelBufferInternal &get_voxels_const() const {
#ifdef DEBUG_ENABLED
		CRASH_COND(_voxels == nullptr);
#endif
		return *_voxels;
	}

	std::shared_ptr<VoxelBufferInternal> get_voxels_shared() const {
#ifdef DEBUG_ENABLED
		CRASH_COND(_voxels == nullptr);
#endif
		return _voxels;
	}

	void set_voxels(std::shared_ptr<VoxelBufferInternal> &buffer) {
		ERR_FAIL_COND(buffer == nullptr);
		_voxels = buffer;
	}

	void set_modified(bool modified) {
#ifdef TOOLS_ENABLED
		if (_modified == false && modified) {
			PRINT_VERBOSE(String("Marking block {0} as modified").format(varray(position.to_vec3())));
		}
#endif
		_modified = modified;
	}

	inline bool is_modified() const {
		return _modified;
	}

	void set_needs_lodding(bool need_lodding) {
		_needs_lodding = need_lodding;
	}

	inline bool get_needs_lodding() const {
		return _needs_lodding;
	}

private:
	VoxelDataBlock(Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer, unsigned int p_lod_index) :
			position(bpos), lod_index(p_lod_index), _voxels(buffer) {}

	std::shared_ptr<VoxelBufferInternal> _voxels;

	// The block was edited, which requires its LOD counterparts to be recomputed
	bool _needs_lodding = false;

	// Indicates if this block is different from the time it was loaded (should be saved)
	bool _modified = false;

	// Tells if it's worth requesting a more precise version of the data.
	// Will be `true` if it's not worth it.
	bool _max_lod_hint = false;
};

#endif // VOXEL_DATA_BLOCK_H
