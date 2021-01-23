#ifndef VOXEL_STREAM_FILE_H
#define VOXEL_STREAM_FILE_H

#include "voxel_block_serializer.h"
#include "voxel_stream.h"

class FileAccess;

// TODO Might be removed in the future, it doesn't add much.

// Helper common base for some file streams.
class VoxelStreamFile : public VoxelStream {
	GDCLASS(VoxelStreamFile, VoxelStream)
protected:
	static void _bind_methods();

	FileAccess *open_file(const String &fpath, int mode_flags, Error *err);

	static thread_local VoxelBlockSerializerInternal _block_serializer;
};

#endif // VOXEL_STREAM_FILE_H
