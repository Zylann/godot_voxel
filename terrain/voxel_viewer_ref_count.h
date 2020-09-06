#ifndef VOXEL_VIEWER_REF_COUNT_H
#define VOXEL_VIEWER_REF_COUNT_H

#include "../util/fixed_array.h"

class VoxelViewerRefCount {
public:
	enum ViewerType {
		TYPE_DATA = 0, // This one is always counted, others are optional
		TYPE_MESH,
		TYPE_COLLISION,
		TYPE_COUNT
	};

	VoxelViewerRefCount() {
		_counts.fill(0);
	}

	inline void add(bool data, bool mesh, bool collision) {
		if (data) {
			add(TYPE_DATA);
		}
		if (mesh) {
			add(TYPE_MESH);
		}
		if (collision) {
			add(TYPE_COLLISION);
		}
	}

	inline void remove(bool data, bool mesh, bool collision) {
		if (data) {
			remove(TYPE_DATA);
		}
		if (mesh) {
			remove(TYPE_MESH);
		}
		if (collision) {
			remove(TYPE_COLLISION);
		}
	}

	inline void add(ViewerType t) {
		++_counts[t];
	}

	inline void remove(ViewerType t) {
		CRASH_COND_MSG(_counts[t] == 0, "Trying to remove a viewer to a block that had none");
		--_counts[t];
	}

	inline uint32_t get(ViewerType t) const {
		return _counts[t];
	}

private:
	FixedArray<uint32_t, TYPE_COUNT> _counts;
};

#endif // VOXEL_VIEWER_REF_COUNT_H
