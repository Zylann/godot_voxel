#ifndef VOXEL_COMPRESSED_DATA_H
#define VOXEL_COMPRESSED_DATA_H

#include "../util/span.h"

namespace VoxelCompressedData {

// Compressed data starts with a single byte telling which compression format is used.
// What follows depends on it.

enum Compression {
	// No compression. All following bytes can be read as-is.
	// Could be used for debugging.
	COMPRESSION_NONE = 0,
	// The next uint32_t will be the size of decompressed data.
	// All following bytes are compressed data using LZ4 defaults.
	// This is the fastest compression format.
	COMPRESSION_LZ4 = 1,
	COMPRESSION_COUNT = 2
};

bool compress(Span<const uint8_t> src, std::vector<uint8_t> &dst, Compression comp);
bool decompress(Span<const uint8_t> src, std::vector<uint8_t> &dst);

} // namespace VoxelCompressedData

#endif // VOXEL_COMPRESSED_DATA_H
