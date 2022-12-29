#ifndef VOXEL_BLOCK_SERIALIZER_H
#define VOXEL_BLOCK_SERIALIZER_H

#include "../util/macros.h"
#include "../util/span.h"

#include <cstdint>
#include <vector>

ZN_GODOT_FORWARD_DECLARE(class StreamPeer)
ZN_GODOT_FORWARD_DECLARE(class FileAccess)
#ifdef ZN_GODOT_EXTENSION
using namespace godot;
#endif

namespace zylann::voxel {

class VoxelBufferInternal;

namespace BlockSerializer {

// Latest version, used when serializing
static const uint8_t BLOCK_FORMAT_VERSION = 4;

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

// Temporary thread-local buffers for internal use
std::vector<uint8_t> &get_tls_data();
std::vector<uint8_t> &get_tls_compressed_data();

} // namespace BlockSerializer
} // namespace zylann::voxel

#endif // VOXEL_BLOCK_SERIALIZER_H
