#ifndef VOXEL_REGION_FILE_UTILS_H
#define VOXEL_REGION_FILE_UTILS_H

#include "../../engine/voxel_engine.h"
#include "../../util/godot/file_utils.h"

namespace zylann::voxel {

inline Error check_directory_created_with_file_locker(const String &fpath_base_dir) {
	VoxelFileLockerWrite file_wlock(zylann::godot::to_std_string(fpath_base_dir));
	return zylann::godot::check_directory_created(fpath_base_dir);
}

} // namespace zylann::voxel

#endif // VOXEL_REGION_FILE_UTILS_H
