#ifndef ZN_HASH_FUNCS_H
#define ZN_HASH_FUNCS_H

#include "math/funcs.h"
#include <cstdint>

namespace zylann {

// Copied from Godot core.

inline uint32_t hash_djb2_one_32(uint32_t p_in, uint32_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

inline uint64_t hash_djb2_one_64(uint64_t p_in, uint64_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

} // namespace zylann

#endif // ZN_HASH_FUNCS_H
