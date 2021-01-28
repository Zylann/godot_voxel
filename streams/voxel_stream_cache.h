#ifndef VOXEL_STREAM_CACHE_H
#define VOXEL_STREAM_CACHE_H

#include "../storage/voxel_buffer.h"

// In-memory database for voxel streams.
// It allows to cache blocks so we can save to the filesystem less frequently, or quickly reload recent blocks.
class VoxelStreamCache {
public:
	struct Block {
		Vector3i position;
		int lod;
		Ref<VoxelBuffer> voxels;
	};

	bool load_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> &out_voxels);
	void save_voxel_block(Vector3i position, uint8_t lod_index, Ref<VoxelBuffer> voxels);

	unsigned int get_indicative_block_count() const;

	template <typename F>
	void flush(F save_func) {
		for (unsigned int lod_index = 0; lod_index < _cache.size(); ++lod_index) {
			Lod &lod = _cache[lod_index];
			RWLockWrite wlock(lod.rw_lock);
			const Vector3i *position = nullptr;
			while ((position = lod.blocks.next(position))) {
				Block *block = lod.blocks.getptr(*position);
				ERR_FAIL_COND(block == nullptr);
				save_func(*block);
			}
			lod.blocks.clear();
		}
	}

private:
	struct Lod {
		HashMap<Vector3i, Block, Vector3iHasher> blocks;
		RWLock *rw_lock;

		Lod() {
			rw_lock = RWLock::create();
		}
		~Lod() {
			memdelete(rw_lock);
		}
	};

	FixedArray<Lod, VoxelConstants::MAX_LOD> _cache;
	unsigned int _count = 0;
};

#endif // VOXEL_STREAM_CACHE_H
