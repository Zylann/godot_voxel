#ifndef VOXEL_BLOCK_KEY_CACHE_H
#define VOXEL_BLOCK_KEY_CACHE_H

#include "../../constants/voxel_constants.h"
#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_unordered_set.h"
#include "../../util/math/vector3i.h"
#include "../../util/thread/rw_lock.h"

namespace zylann::voxel {

struct BlockKeysCache {
	FixedArray<StdUnorderedSet<Vector3i>, constants::MAX_LOD> lods;
	RWLock rw_lock;

	inline bool contains(Vector3i bpos, unsigned int lod_index) const {
		const StdUnorderedSet<Vector3i> &keys = lods[lod_index];
		RWLockRead rlock(rw_lock);
		return keys.find(bpos) != keys.end();
	}

	inline void add_no_lock(Vector3i bpos, unsigned int lod_index) {
		lods[lod_index].insert(bpos);
	}

	inline void add(Vector3i bpos, unsigned int lod_index) {
		RWLockWrite wlock(rw_lock);
		add_no_lock(bpos, lod_index);
	}

	inline void clear() {
		RWLockWrite wlock(rw_lock);
		for (unsigned int i = 0; i < lods.size(); ++i) {
			lods[i].clear();
		}
	}

	// inline size_t get_memory_usage() const {
	// 	size_t mem = 0;
	// 	for (unsigned int i = 0; i < lods.size(); ++i) {
	// 		const StdUnorderedSet<Vector3i> &keys = lods[i];
	// 		mem += sizeof(Vector3i) * keys.size();
	// 	}
	// 	return mem;
	// }
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCK_KEY_CACHE_H
