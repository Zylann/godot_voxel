#ifndef ZN_GODOT_VARIANT_H
#define ZN_GODOT_VARIANT_H

#if defined(ZN_GODOT)
#include <core/variant/variant.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/variant/variant.hpp>
using namespace godot;
#endif

#include "../../span.h"

namespace zylann {

size_t get_variant_encoded_size(const Variant &src);
size_t encode_variant(const Variant &src, Span<uint8_t> dst);
bool decode_variant(Span<const uint8_t> src, Variant &dst, size_t &out_read_size);

} // namespace zylann

#endif // ZN_GODOT_VARIANT_H
