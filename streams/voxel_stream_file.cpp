#include "voxel_stream_file.h"
#include "../server/voxel_server.h"
#include "../util/profiling.h"
#include <core/os/file_access.h>
#include <core/os/os.h>

thread_local VoxelBlockSerializerInternal VoxelStreamFile::_block_serializer;

FileAccess *VoxelStreamFile::open_file(const String &fpath, int mode_flags, Error *err) {
	VOXEL_PROFILE_SCOPE();
	//uint64_t time_before = OS::get_singleton()->get_ticks_usec();
	FileAccess *f = FileAccess::open(fpath, mode_flags, err);
	//uint64_t time_spent = OS::get_singleton()->get_ticks_usec() - time_before;
	//_stats.time_spent_opening_files += time_spent;
	//++_stats.file_openings;
	return f;
}

void VoxelStreamFile::_bind_methods() {
}
