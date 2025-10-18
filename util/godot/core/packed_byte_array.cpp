#include "packed_byte_array.h"

namespace zylann::godot {
namespace PackedByteArrayUtility {

PackedByteArray compress(const PackedByteArray &self, const FileAccess::CompressionMode p_mode) {
#if defined(ZN_GODOT)
	PackedByteArray compressed;

	if (self.size() > 0) {
		const Compression::Mode mode = static_cast<Compression::Mode>(p_mode);
		compressed.resize(Compression::get_max_compressed_buffer_size(self.size(), mode));
		const int64_t result = Compression::compress(compressed.ptrw(), self.ptr(), self.size(), mode);

		const int64_t compressed_size = result >= 0 ? result : 0;
		compressed.resize(compressed_size);
	}

	return compressed;

#elif defined(ZN_GODOT_EXTENSION)
	return self.compress(p_mode);
#endif
}

PackedByteArray decompress(
		const PackedByteArray &self,
		const int64_t buffer_size,
		const FileAccess::CompressionMode p_mode
) {
#if defined(ZN_GODOT)
	PackedByteArray decompressed;
	const Compression::Mode mode = static_cast<Compression::Mode>(p_mode);

	if (buffer_size <= 0) {
		ERR_FAIL_V_MSG(decompressed, "Decompression buffer size must be greater than zero.");
	}
	if (self.size() == 0) {
		ERR_FAIL_V_MSG(decompressed, "Compressed buffer size must be greater than zero.");
	}

	decompressed.resize(buffer_size);
	const int64_t result = Compression::decompress(decompressed.ptrw(), buffer_size, self.ptr(), self.size(), mode);

	const int64_t decompressed_size = result >= 0 ? result : 0;
	decompressed.resize(decompressed_size);

	return decompressed;

#elif defined(ZN_GODOT_EXTENSION)
	return self.decompress(buffer_size, p_mode);
#endif
}

} // namespace PackedByteArrayUtility
} // namespace zylann::godot
