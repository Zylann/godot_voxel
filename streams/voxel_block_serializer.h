#ifndef VOXEL_BLOCK_SERIALIZER_H
#define VOXEL_BLOCK_SERIALIZER_H

#include "../util/span.h"

#include <vector>

class StreamPeer;
class FileAccess;

namespace zylann::voxel {

class VoxelBufferInternal;

namespace BlockSerializer {

struct SerializeResult {
	// The lifetime of the pointed object is only valid in the calling thread,
	// until another serialization or deserialization call is made
	const std::vector<uint8_t> &data;
	bool success;

	inline SerializeResult(const std::vector<uint8_t> &p_data, bool p_success) : data(p_data), success(p_success) {}
};

SerializeResult serialize(const VoxelBufferInternal &voxel_buffer);
bool deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer);

SerializeResult serialize_and_compress(const VoxelBufferInternal &voxel_buffer);
bool decompress_and_deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer);
bool decompress_and_deserialize(FileAccess &f, unsigned int size_to_read, VoxelBufferInternal &out_voxel_buffer);

int serialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, bool compress);
void deserialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, int size, bool decompress);

} // namespace BlockSerializer
} // namespace zylann::voxel

#endif // VOXEL_BLOCK_SERIALIZER_H
