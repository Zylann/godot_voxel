#ifndef VOXEL_VIEWER_REF_COUNT_H
#define VOXEL_VIEWER_REF_COUNT_H

#include "../util/fixed_array.h"

class VoxelRefCount {
public:
	inline void add() {
		++_count;
	}

	inline void remove() {
		ERR_FAIL_COND_MSG(_count == 0, "Trying to decrease refcount when it's already zero");
		--_count;
	}

	inline unsigned int get() const {
		return _count;
	}

private:
	unsigned int _count = 0;
};

#endif // VOXEL_VIEWER_REF_COUNT_H
