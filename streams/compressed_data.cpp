#include "compressed_data.h"
#include "../thirdparty/lz4/lz4.h"
#include "../util/godot/core/packed_arrays.h"
#include "../util/godot/core/packed_byte_array.h"
#include "../util/io/serialization.h"
#include "../util/profiling.h"
#include "../util/string/format.h"

#include <limits>

namespace zylann::voxel::CompressedData {

bool decompress_lz4(MemoryReader &f, Span<const uint8_t> src, StdVector<uint8_t> &dst) {
	const int64_t decompressed_size = f.get_32();
	ZN_ASSERT_RETURN_V(decompressed_size >= 0, false);

	const int header_size = sizeof(uint8_t) + sizeof(uint32_t);

	dst.resize(decompressed_size);

	const int64_t actually_decompressed_size = LZ4_decompress_safe(
			(const char *)src.data() + header_size, (char *)dst.data(), src.size() - header_size, dst.size()
	);

	ZN_ASSERT_RETURN_V_MSG(
			actually_decompressed_size >= 0, false, format("LZ4 decompression error {}", actually_decompressed_size)
	);

	ZN_ASSERT_RETURN_V_MSG(
			actually_decompressed_size == decompressed_size,
			false,
			format("Expected {} bytes, obtained {}", decompressed_size, actually_decompressed_size)
	);

	return true;
}

bool decompress_gd(MemoryReader &f, StdVector<uint8_t> &dst, const FileAccess::CompressionMode mode) {
	const int64_t decompressed_size = f.get_32();
	ZN_ASSERT_RETURN_V(decompressed_size >= 0, false);

	const Span<const uint8_t> src_comp = f.data.sub(f.pos);
	PackedByteArray pba_src;
	pba_src.resize(src_comp.size());
	zylann::godot::copy_to(pba_src, src_comp);

	const PackedByteArray pba_dst = zylann::godot::PackedByteArrayUtility::decompress(pba_src, decompressed_size, mode);

	const int64_t actually_decompressed_size = pba_dst.size();
	ZN_ASSERT_RETURN_V_MSG(
			actually_decompressed_size == decompressed_size,
			false,
			format("Expected {} bytes, obtained {}", decompressed_size, actually_decompressed_size)
	);

	dst.resize(decompressed_size);
	zylann::godot::copy_to(to_span(dst), pba_dst);

	return true;
}

bool decompress(Span<const uint8_t> src, StdVector<uint8_t> &dst) {
	ZN_PROFILE_SCOPE();

	MemoryReader f(src, ENDIANNESS_LITTLE_ENDIAN);

	const Compression comp = static_cast<Compression>(f.get_8());
	ZN_ASSERT_RETURN_V(comp >= 0 && comp < COMPRESSION_COUNT, false);

	switch (comp) {
		case COMPRESSION_NONE: {
			// We still have to do a copy. The point of this container is compression,
			// so we don't worry too much about the performance impact of not using `src` directly.
			dst.resize(src.size() - 1);
			memcpy(dst.data(), src.data() + 1, dst.size());
		} break;

		case COMPRESSION_LZ4_BE:
			// Legacy format
			f.endianness = ENDIANNESS_BIG_ENDIAN;
			ZN_ASSERT_RETURN_V(decompress_lz4(f, src, dst), false);
			break;

		case COMPRESSION_LZ4:
			ZN_ASSERT_RETURN_V(decompress_lz4(f, src, dst), false);
			break;

		case COMPRESSION_ZSTD:
			ZN_ASSERT_RETURN_V(decompress_gd(f, dst, FileAccess::COMPRESSION_ZSTD), false);
			break;

		default:
			ZN_PRINT_ERROR("Invalid compression header");
			return false;
	}

	return true;
}

bool compress_lz4(MemoryWriter &f, Span<const uint8_t> src, StdVector<uint8_t> &dst) {
	ZN_ASSERT_RETURN_V(src.size() <= std::numeric_limits<uint32_t>::max(), false);

	f.store_32(src.size());

	const uint32_t header_size = sizeof(uint8_t) + sizeof(uint32_t);
	dst.resize(header_size + LZ4_compressBound(src.size()));

	const uint32_t compressed_size = LZ4_compress_default(
			(const char *)src.data(), (char *)dst.data() + header_size, src.size(), dst.size() - header_size
	);

	ZN_ASSERT_RETURN_V(int(compressed_size) >= 0, false);
	ZN_ASSERT_RETURN_V(compressed_size != 0, false);

	dst.resize(header_size + compressed_size);

	return true;
}

bool compress_gd(MemoryWriter &f, Span<const uint8_t> src, const FileAccess::CompressionMode mode) {
	// Falling back on Godot here, which is simple, but has overhead and more memory allocations.

	f.store_32(src.size());

	PackedByteArray pba_src;
	pba_src.resize(src.size());
	zylann::godot::copy_to(pba_src, src);

	const PackedByteArray pba_dst =
			zylann::godot::PackedByteArrayUtility::compress(pba_src, FileAccess::COMPRESSION_ZSTD);

	const int64_t compressed_size = pba_dst.size();
	ZN_ASSERT_RETURN_V(compressed_size > 0, false);

	f.store_buffer(to_span(pba_dst));

	return true;
}

bool compress(Span<const uint8_t> src, StdVector<uint8_t> &dst, const Compression comp) {
	ZN_PROFILE_SCOPE();

	switch (comp) {
		case COMPRESSION_NONE: {
			dst.resize(src.size() + 1);
			dst[0] = comp;
			memcpy(dst.data() + 1, src.data(), src.size());
		} break;

		case COMPRESSION_LZ4_BE: {
			ZN_PRINT_ERROR("Using deprecated LZ4_BE compression!");
			dst.clear();
			MemoryWriter f(dst, ENDIANNESS_LITTLE_ENDIAN);
			f.store_8(comp);
			compress_lz4(f, src, dst);
		} break;

		case COMPRESSION_LZ4: {
			// Write header
			// Must clear first because MemoryWriter writes from the end
			dst.clear();
			MemoryWriter f(dst, ENDIANNESS_LITTLE_ENDIAN);
			f.store_8(comp);
			compress_lz4(f, src, dst);
		} break;

		case COMPRESSION_ZSTD: {
			dst.clear();
			MemoryWriter f(dst, ENDIANNESS_LITTLE_ENDIAN);
			f.store_8(comp);
			compress_gd(f, src, FileAccess::COMPRESSION_ZSTD);
		} break;

		default:
			ZN_PRINT_ERROR(format("Invalid compression header {}", comp));
			return false;
	}

	return true;
}

} // namespace zylann::voxel::CompressedData
