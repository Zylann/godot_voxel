#include "compressed_data.h"
#include "../thirdparty/lz4/lz4.h"
#include "../util/profiling.h"
#include "../util/serialization.h"

#include <core/io/file_access_memory.h>
#include <core/variant.h>
#include <limits>

namespace VoxelCompressedData {

bool decompress(Span<const uint8_t> src, std::vector<uint8_t> &dst) {
	VOXEL_PROFILE_SCOPE();

	// TODO Apparently big-endian is dead
	// I chose it originally to match "network byte order",
	// but as I read comments about it there seem to be no reason to continue using it. Needs a version increment.
	VoxelUtility::MemoryReader f(src, VoxelUtility::ENDIANESS_BIG_ENDIAN);

	const Compression comp = static_cast<Compression>(f.get_8());
	ERR_FAIL_INDEX_V(comp, COMPRESSION_COUNT, false);

	switch (comp) {
		case COMPRESSION_NONE: {
			// We still have to do a copy. The point of this container is compression,
			// so we don't worry too much about the performance impact of not using `src` directly.
			dst.resize(src.size() - 1);
			memcpy(dst.data(), src.data() + 1, dst.size());
		} break;

		case COMPRESSION_LZ4: {
			const uint32_t decompressed_size = f.get_32();
			const uint32_t header_size = sizeof(uint8_t) + sizeof(uint32_t);

			dst.resize(decompressed_size);

			const uint32_t actually_decompressed_size = LZ4_decompress_safe(
					(const char *)src.data() + header_size,
					(char *)dst.data(),
					src.size() - header_size,
					dst.size());

			ERR_FAIL_COND_V_MSG(actually_decompressed_size < 0, false,
					String("LZ4 decompression error {0}").format(varray(actually_decompressed_size)));

			ERR_FAIL_COND_V_MSG(actually_decompressed_size != decompressed_size, false,
					String("Expected {0} bytes, obtained {1}")
							.format(varray(decompressed_size, actually_decompressed_size)));
		} break;

		default:
			ERR_PRINT("Invalid compression header");
			return false;
	}

	return true;
}

bool compress(Span<const uint8_t> src, std::vector<uint8_t> &dst, Compression comp) {
	VOXEL_PROFILE_SCOPE();

	switch (comp) {
		case COMPRESSION_NONE: {
			dst.resize(src.size() + 1);
			dst[0] = comp;
			memcpy(dst.data() + 1, src.data(), src.size());
		} break;

		case COMPRESSION_LZ4: {
			ERR_FAIL_COND_V(src.size() > std::numeric_limits<uint32_t>::max(), false);

			// Write header
			// Must clear first because MemoryWriter writes from the end
			dst.clear();
			VoxelUtility::MemoryWriter f(dst, VoxelUtility::ENDIANESS_BIG_ENDIAN);
			f.store_8(comp);
			f.store_32(src.size());

			const uint32_t header_size = sizeof(uint8_t) + sizeof(uint32_t);
			dst.resize(header_size + LZ4_compressBound(src.size()));

			const uint32_t compressed_size = LZ4_compress_default(
					(const char *)src.data(),
					(char *)dst.data() + header_size,
					src.size(),
					dst.size() - header_size);

			ERR_FAIL_COND_V(compressed_size < 0, false);
			ERR_FAIL_COND_V(compressed_size == 0, false);

			dst.resize(header_size + compressed_size);
		} break;

		default:
			ERR_PRINT("Invalid compression header");
			return false;
	}

	return true;
}

} // namespace VoxelCompressedData
