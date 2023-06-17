#include "voxel_spatial_lock.h"
#include "../util/log.h"
#include "../util/string_funcs.h"

namespace zylann::voxel {

VoxelSpatialLock::VoxelSpatialLock() {
	_boxes.reserve(8);
}

void VoxelSpatialLock::remove_box(const BoxBounds3i &box, Mode mode) {
#ifdef VOXEL_SPATIAL_LOCK_CHECKS
	const Thread::ID thread_id = Thread::get_caller_id();
#endif

	for (unsigned int i = 0; i < _boxes.size(); ++i) {
		const Box &existing_box = _boxes[i];

		if (existing_box.bounds == box && existing_box.mode == mode
#ifdef VOXEL_SPATIAL_LOCK_CHECKS
				&& existing_box.thread_id == thread_id
#endif
		) {
			_boxes[i] = _boxes[_boxes.size() - 1];
			_boxes.pop_back();
			return;
		}
	}
	// Could be a bug
	ZN_PRINT_ERROR(format("Could not find box to remove {} with mode {}", box, mode));
}

} // namespace zylann::voxel
