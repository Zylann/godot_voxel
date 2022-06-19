#ifndef ZN_VECTOR3I16_H
#define ZN_VECTOR3I16_H

#include <cstdint>
#include <functional>

namespace zylann {

struct Vector3i16 {
	int16_t x;
	int16_t y;
	int16_t z;

	Vector3i16() : x(0), y(0), z(0) {}
	Vector3i16(int16_t p_x, int16_t p_y, int16_t p_z) : x(p_x), y(p_y), z(p_z) {}

	inline bool operator==(const Vector3i16 p_v) const {
		return x == p_v.x && y == p_v.y && z == p_v.z;
	}

	inline bool operator!=(const Vector3i16 p_v) const {
		return x != p_v.x || y != p_v.y || z != p_v.z;
	}
};

inline size_t get_hash_st(const zylann::Vector3i16 &v) {
	// TODO Optimization: benchmark this hash, I just wanted one that works
	uint64_t m = 0;
	*(zylann::Vector3i16 *)m = v;
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
