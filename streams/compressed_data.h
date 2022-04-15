#ifndef VOXEL_COMPRESSED_DATA_H
#define VOXEL_COMPRESSED_DATA_H

#include "../util/span.h"
#include <cstdint>

namespace zylann::voxel::CompressedData {

// Compressed data starts with a single byte telling which compression format is used.
// What follows depends on it.

enum Compression {
	// No compression. All following bytes can be read as-is.
	// Could be used for debugging.
	COMPRESSION_NONE = 0,
	// [deprecated]
	// The next uint32_t will be the size of decompressed data in big endian format.
	// All following bytes are compressed data using LZ4 defaults.
	// This is the fastest compression format.
	COMPRESSION_LZ4_BE = 1,
	// The next uint32_t will be the size of decompressed data (little endian).
	// All following bytes are compressed data using LZ4 defaults.
	// This is the fastest compression format.
	COMPRESSION_LZ4 = 2,
	COMPRESSION_COUNT = 3
};

bool compress(Span<const uint8_t> src, std::vector<uint8_t> &dst, Compression comp);
bool decompress(Span<const uint8_t> src, std::vector<uint8_t> &dst);

} // namespace zylann::voxel::CompressedData

#endif // VOXEL_COMPRESSED_DATA_H
