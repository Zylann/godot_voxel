#ifndef VOXEL_BLOCK_SERIALIZER_H
#define VOXEL_BLOCK_SERIALIZER_H

#include "../util/containers/span.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/macros.h"

#include <cstdint>

ZN_GODOT_FORWARD_DECLARE(class FileAccess)
#ifdef ZN_GODOT_EXTENSION
using namespace godot;
#endif

namespace zylann::voxel {

class VoxelBuffer;

namespace BlockSerializer {

// Latest version, used when serializing
static const uint8_t BLOCK_FORMAT_VERSION = 4;

struct SerializeResult {
	// The lifetime of the pointed object is only valid in the calling thread,
	// until another serialization or deserialization call is made.
	// TODO Eventually figure out allocators so the caller can decide
	const StdVector<uint8_t> &data;
	bool success;

	inline SerializeResult(const StdVector<uint8_t> &p_data, bool p_success) : data(p_data), success(p_success) {}
};

SerializeResult serialize(const VoxelBuffer &voxel_buffer);
bool deserialize(Span<const uint8_t> p_data, VoxelBuffer &out_voxel_buffer);

SerializeResult serialize_and_compress(const VoxelBuffer &voxel_buffer);
bool decompress_and_deserialize(Span<const uint8_t> p_data, VoxelBuffer &out_voxel_buffer);
bool decompress_and_deserialize(FileAccess &f, unsigned int size_to_read, VoxelBuffer &out_voxel_buffer);

// Temporary thread-local buffers for internal use
StdVector<uint8_t> &get_tls_data();
StdVector<uint8_t> &get_tls_compressed_data();

} // namespace BlockSerializer
} // namespace zylann::voxel

#endif // VOXEL_BLOCK_SERIALIZER_H
