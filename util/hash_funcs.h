#ifndef ZN_HASH_FUNCS_H
#define ZN_HASH_FUNCS_H

#include "math/funcs.h"
#include <cstdint>

namespace zylann {

// Copied from Godot core.
// TODO GodotCpp now has these functions too, include instead? Or keep using our own custom set?

inline uint32_t hash_djb2_one_32(uint32_t p_in, uint32_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

inline uint64_t hash_djb2_one_64(uint64_t p_in, uint64_t p_prev = 5381) {
	return ((p_prev << 5) + p_prev) ^ p_in;
}

#define HASH_MURMUR3_SEED 0x7F07C65
// Murmurhash3 32-bit version.
// All MurmurHash versions are public domain software, and the author disclaims all copyright to their code.

inline uint32_t hash_murmur3_one_32(uint32_t p_in, uint32_t p_seed = HASH_MURMUR3_SEED) {
	p_in *= 0xcc9e2d51;
	p_in = (p_in << 15) | (p_in >> 17);
	p_in *= 0x1b873593;

	p_seed ^= p_in;
	p_seed = (p_seed << 13) | (p_seed >> 19);
	p_seed = p_seed * 5 + 0xe6546b64;

	return p_seed;
}

inline uint32_t hash_fmix32(uint32_t h) {
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

} // namespace zylann

#endif // ZN_HASH_FUNCS_H
