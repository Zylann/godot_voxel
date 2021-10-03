#ifndef VOXEL_BLOCK_SERIALIZER_H
#define VOXEL_BLOCK_SERIALIZER_H

#include "../util/span.h"

#include <core/io/file_access_memory.h>
#include <core/reference.h>
#include <vector>

class StreamPeer;
class VoxelBufferInternal;

class VoxelBlockSerializerInternal {
	// Had to be named differently to not conflict with the wrapper for Godot script API
public:
	struct SerializeResult {
		const std::vector<uint8_t> &data;
		bool success;

		inline SerializeResult(const std::vector<uint8_t> &p_data, bool p_success) :
				data(p_data), success(p_success) {}
	};

	SerializeResult serialize(const VoxelBufferInternal &voxel_buffer);
	bool deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer);

	SerializeResult serialize_and_compress(const VoxelBufferInternal &voxel_buffer);
	bool decompress_and_deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer);
	bool decompress_and_deserialize(FileAccess *f, unsigned int size_to_read, VoxelBufferInternal &out_voxel_buffer);

	int serialize(Ref<StreamPeer> peer, VoxelBufferInternal &voxel_buffer, bool compress);
	void deserialize(Ref<StreamPeer> peer, VoxelBufferInternal &voxel_buffer, int size, bool decompress);

private:
	// Make thread-locals?
	std::vector<uint8_t> _data;
	std::vector<uint8_t> _compressed_data;
	std::vector<uint8_t> _metadata_tmp;
	FileAccessMemory _file_access_memory;
};

class VoxelBuffer;

class VoxelBlockSerializer : public Reference {
	GDCLASS(VoxelBlockSerializer, Reference)
public:
	int serialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, bool compress);
	void deserialize(Ref<StreamPeer> peer, Ref<VoxelBuffer> voxel_buffer, int size, bool decompress);

private:
	static void _bind_methods();

	VoxelBlockSerializerInternal _serializer;
};

#endif // VOXEL_BLOCK_SERIALIZER_H
