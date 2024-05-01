#ifndef ZN_VECTOR3I16_H
#define ZN_VECTOR3I16_H

#include "vector3t.h"
#include <cstdint>
#include <functional>

namespace zylann {

typedef Vector3T<int16_t> Vector3i16;

inline size_t get_hash_st(const zylann::Vector3i16 &v) {
	// TODO Optimization: benchmark this hash, I just wanted one that works
	// static_assert(sizeof(zylann::Vector3i16) <= sizeof(uint64_t));
	const uint64_t m = v.x | (static_cast<uint64_t>(v.y) << 16) | (static_cast<uint64_t>(v.z) << 32);
	return std::hash<uint64_t>{}(m);
}

} // namespace zylann

// For STL
namespace std {
template <>
struct hash<zylann::Vector3i16> {
	size_t operator()(const zylann::Vector3i16 &v) const {
		return zylann::get_hash_st(v);
	}
};
} // namespace std

#endif // ZN_VECTOR3I16_H
