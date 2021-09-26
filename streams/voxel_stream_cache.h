#ifndef VOXEL_STREAM_CACHE_H
#define VOXEL_STREAM_CACHE_H

#include "../storage/voxel_buffer.h"
#include "instance_data.h"
#include <memory>
#include <unordered_map>

// In-memory database for voxel streams.
// It allows to cache blocks so we can save to the filesystem later less frequently, or quickly reload recent blocks.
class VoxelStreamCache {
public:
	struct Block {
		Vector3i position;
		int lod;

		// Absence of voxel data can mean two things:
		// - Voxel data has been erased (use case not really implemented yet, but may happen in the future)
		// - Voxel data has never been saved over, so should be left untouched
		bool has_voxels = false;
		bool voxels_deleted = false;

		VoxelBufferInternal voxels;
		std::unique_ptr<VoxelInstanceBlockData> instances;
	};

	// Copies cached block into provided buffer
	bool load_voxel_block(Vector3i position, uint8_t lod_index, VoxelBufferInternal &out_voxels);

	// Stores provided block into the cache. The cache will take ownership of the provided data.
	void save_voxel_block(Vector3i position, uint8_t lod_index, VoxelBufferInternal &voxels);

	// Copies cached data into the provided pointer. A new instance will be made if found.
	bool load_instance_block(
			Vector3i position, uint8_t lod_index, std::unique_ptr<VoxelInstanceBlockData> &out_instances);

	// Stores provided block into the cache. The cache will take ownership of the provided data.
	void save_instance_block(Vector3i position, uint8_t lod_index, std::unique_ptr<VoxelInstanceBlockData> instances);

	unsigned int get_indicative_block_count() const;

	template <typename F>
	void flush(F save_func) {
		_count = 0;
		for (unsigned int lod_index = 0; lod_index < _cache.size(); ++lod_index) {
			Lod &lod = _cache[lod_index];
			RWLockWrite wlock(lod.rw_lock);
			for (auto it = lod.blocks.begin(); it != lod.blocks.end(); ++it) {
				Block &block = it->second;
				save_func(block);
			}
			lod.blocks.clear();
		}
	}

private:
	struct Lod {
		// Not using pointers for values, since unordered_map does not invalidate pointers to values
		std::unordered_map<Vector3i, Block> blocks;
		RWLock rw_lock;
	};

	FixedArray<Lod, VoxelConstants::MAX_LOD> _cache;
	unsigned int _count = 0;
};

#endif // VOXEL_STREAM_CACHE_H
