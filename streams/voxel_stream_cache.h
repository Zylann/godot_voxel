#ifndef VOXEL_STREAM_CACHE_H
#define VOXEL_STREAM_CACHE_H

#include "../storage/voxel_buffer.h"
#include "instance_data.h"
#include <memory>
#include <unordered_map>

// In-memory database for voxel streams.
// It allows to cache blocks so we can save to the filesystem less frequently, or quickly reload recent blocks.
class VoxelStreamCache {
public:
	struct Block {
		Vector3i position;
		int lod;

		// Because `voxels` being null has two possible meanings:
		// - true: Voxel data has been erased
		// - false: Voxel data should be left untouched
		bool has_voxels = false;

		Ref<VoxelBuffer> voxels;
		std::unique_ptr<VoxelInstanceBlockData> instances;
	};

	// Copies cached block into provided buffer
	bool load_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> &out_voxels);

	// Stores provided block into the cache. The cache will take ownership of the provided data.
	void save_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> voxels);

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
		std::unordered_map<Vector3i, Block> blocks;
		RWLock rw_lock;
	};

	FixedArray<Lod, VoxelConstants::MAX_LOD> _cache;
	unsigned int _count = 0;
};

#endif // VOXEL_STREAM_CACHE_H
