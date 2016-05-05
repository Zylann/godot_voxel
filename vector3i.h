#ifndef VOXEL_VECTOR3I_H
#define VOXEL_VECTOR3I_H

#include <core/math/vector3.h>

struct Vector3i {

    int x;
    int y;
    int z;

    _FORCE_INLINE_ Vector3i() : x(0), y(0), z(0) {}

    _FORCE_INLINE_ Vector3i(int px, int py, int pz) : x(px), y(py), z(pz) {}

    _FORCE_INLINE_ Vector3i(const Vector3i & other) {
        *this = other;
    }

    _FORCE_INLINE_ Vector3i(const Vector3 & f) {
        x = Math::floor(f.x);
        y = Math::floor(f.y);
        z = Math::floor(f.z);
    }

    _FORCE_INLINE_ Vector3 to_vec3() {
        return Vector3(x, y, z);
    }

    _FORCE_INLINE_ Vector3i & operator=(const Vector3i & other) {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    _FORCE_INLINE_ Vector3i operator+(const Vector3i & other) const {
        return Vector3i(x + other.x, y + other.y, z + other.z);
    }

    _FORCE_INLINE_ Vector3i operator-(const Vector3i & other) const {
        return Vector3i(x - other.x, y - other.y, z - other.z);
    }

    _FORCE_INLINE_ Vector3i operator*(int n) const {
        return Vector3i(x * n, y * n, z * n);
    }

    _FORCE_INLINE_ Vector3i operator/(int n) const {
        return Vector3i(x / n, y / n, z / n);
    }

    _FORCE_INLINE_ bool operator==(const Vector3i & other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    _FORCE_INLINE_ bool operator!=(const Vector3i & other) const {
        return x != other.x && y != other.y && z != other.z;
    }

    void clamp_to(const Vector3i min, const Vector3i max) {
        if (x < min.x) x = min.x;
        if (y < min.y) y = min.y;
        if (z < min.z) z = min.z;

        if (x >= max.x) x = max.x;
        if (y >= max.y) y = max.y;
        if (z >= max.z) z = max.z;
    }


};

struct Vector3iHasher {
	static _FORCE_INLINE_ uint32_t hash(const Vector3i & v) {
		uint32_t hash = hash_djb2_one_32(v.x);
		hash = hash_djb2_one_32(v.y);
		return hash_djb2_one_32(v.z);
    }
};

#endif // VOXEL_VECTOR3I_H

