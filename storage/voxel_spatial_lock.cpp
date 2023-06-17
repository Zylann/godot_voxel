#include "voxel_spatial_lock.h"
#include "../util/log.h"
#include "../util/string_funcs.h"

namespace zylann::voxel {

VoxelSpatialLock::VoxelSpatialLock() {
	_boxes.reserve(8);
}

void VoxelSpatialLock::remove_box(const BoxBounds3i &box, Mode mode) {
	for (unsigned int i = 0; i < _boxes.size(); ++i) {
		const Box &existing_box = _boxes[i];
		if (existing_box.bounds == box && existing_box.mode == mode) {
			_boxes[i] = _boxes[_boxes.size() - 1];
			_boxes.pop_back();
			return;
		}
	}
	// Could be a bug
	ZN_PRINT_ERROR(format("Could not find box to remove {} with mode {}", box, mode));
}

} // namespace zylann::voxel
