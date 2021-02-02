#include "voxel_stream_cache.h"

bool VoxelStreamCache::load_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> &out_voxels) {
	const Lod &lod = _cache[lod_index];
	lod.rw_lock->read_lock();
	const Block *block_ptr = lod.blocks.getptr(position);

	if (block_ptr == nullptr) {
		// Not in cache, will have to query
		lod.rw_lock->read_unlock();
		return false;

	} else {
		// In cache, serve it
		out_voxels = block_ptr->voxels;
		lod.rw_lock->read_unlock();
		return true;
	}
}

void VoxelStreamCache::save_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> voxels) {
	Lod &lod = _cache[lod_index];
	RWLockWrite wlock(lod.rw_lock);
	Block *block_ptr = lod.blocks.getptr(position);

	if (block_ptr == nullptr) {
		// Not cached yet, create an entry
		Block b;
		b.position = position;
		b.lod = lod_index;
		b.voxels = voxels;
		lod.blocks.set(position, b);
		++_count;

	} else {
		// Cached already, overwrite
		block_ptr->voxels = voxels;
	}
}

unsigned int VoxelStreamCache::get_indicative_block_count() const {
	return _count;
}
