#ifndef VOXEL_BLOCK_SERIALIZER_GD_H
#define VOXEL_BLOCK_SERIALIZER_GD_H

#include "../storage/voxel_buffer_gd.h"
#include "compressed_data.h"

ZN_GODOT_FORWARD_DECLARE(class StreamPeer);

namespace zylann::voxel::godot {

class VoxelBuffer;

// Godot-facing API for BlockSerializer
// TODO Could be a singleton? Or methods on VoxelBuffer? This object has no state.
class VoxelBlockSerializer : public RefCounted {
	GDCLASS(VoxelBlockSerializer, RefCounted)
public:
	enum Compression {
		COMPRESSION_NONE,
		COMPRESSION_LZ4,
		COMPRESSION_ZSTD,
	};

	// Must use this because the internal enum does not have consecutive IDs (can't change because of saves),
	// and Godot wants consecutive IDs...
	static Compression compression_to_gd(const CompressedData::Compression src);
	static CompressedData::Compression compression_from_gd(const Compression src);

	static const char *COMPRESSION_MODE_HINT_STRING;

	static int serialize_to_stream_peer(
			Ref<StreamPeer> peer,
			Ref<VoxelBuffer> voxel_buffer,
			const Compression compress_mode
	);

	static void deserialize_from_stream_peer(
			Ref<StreamPeer> peer,
			Ref<VoxelBuffer> voxel_buffer,
			const int64_t size,
			const bool compress_mode
	);

	static PackedByteArray serialize_to_byte_array(Ref<VoxelBuffer> voxel_buffer, const Compression compress_mode);

	static void deserialize_from_byte_array(
			PackedByteArray bytes,
			Ref<VoxelBuffer> voxel_buffer,
			const bool decompress
	);

	static void _bind_methods();
};

} // namespace zylann::voxel::godot

VARIANT_ENUM_CAST(zylann::voxel::godot::VoxelBlockSerializer::Compression);

#endif // VOXEL_BLOCK_SERIALIZER_GD_H
