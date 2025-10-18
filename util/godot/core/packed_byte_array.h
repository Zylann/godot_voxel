#ifndef ZN_GODOT_PACKED_BYTE_ARRAY_H
#define ZN_GODOT_PACKED_BYTE_ARRAY_H

#if defined(ZN_GODOT)
#include <core/io/file_access.h>
#include <core/variant/variant.h>

#elif defined(ZN_GODOT_EXTENSION)
#include "../classes/file_access.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
using namespace godot;
#endif

namespace zylann::godot {
namespace PackedByteArrayUtility {

PackedByteArray compress(const PackedByteArray &self, const FileAccess::CompressionMode p_mode);

PackedByteArray decompress(
		const PackedByteArray &self,
		const int64_t buffer_size,
		const FileAccess::CompressionMode p_mode
);

} // namespace PackedByteArrayUtility
} // namespace zylann::godot

#endif // ZN_GODOT_PACKED_BYTE_ARRAY_H
