#ifndef VOXEL_VECTOR3I_H
#define VOXEL_VECTOR3I_H

#include <core/math/vector3.h>

struct Vector3i {

    union {
        struct {
            int x;
            int y;
            int z;
        };
        int coords[3];
    };

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

    _FORCE_INLINE_ void operator+=(const Vector3i & other) {
        x += other.x;
        y += other.y;
        z += other.z;
    }

    _FORCE_INLINE_ void operator-=(const Vector3i & other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
    }

    _FORCE_INLINE_ int & operator[](unsigned int i) {
        return coords[i];
    }

    void clamp_to(const Vector3i min, const Vector3i max) {
        if (x < min.x) x = min.x;
        if (y < min.y) y = min.y;
        if (z < min.z) z = min.z;

        if (x >= max.x) x = max.x;
        if (y >= max.y) y = max.y;
        if (z >= max.z) z = max.z;
    }

    _FORCE_INLINE_ bool is_contained_in(const Vector3i & min, const Vector3i & max) {
        return x >= min.x && y >= min.y && z >= min.z
            && x < max.x && y < max.y && z < max.z;
    }

    _FORCE_INLINE_ Vector3i wrap(const Vector3i & size) {
        return Vector3i(
            x % size.x,
            y % size.y,
            z % size.z
        );
    }

};

_FORCE_INLINE_ Vector3i operator+(const Vector3i a, const Vector3i & b) {
    return Vector3i(a.x + b.x, a.y + b.y, a.z + b.z);
}

_FORCE_INLINE_ Vector3i operator-(const Vector3i & a, const Vector3i & b) {
    return Vector3i(a.x - b.x, a.y - b.y, a.z - b.z);
}

_FORCE_INLINE_ Vector3i operator*(const Vector3i & a, int n) {
    return Vector3i(a.x * n, a.y * n, a.z * n);
}

_FORCE_INLINE_ Vector3i operator*(int n, const Vector3i & a) {
    return Vector3i(a.x * n, a.y * n, a.z * n);
}

_FORCE_INLINE_ Vector3i operator/(const Vector3i & a, int n) {
    return Vector3i(a.x / n, a.y / n, a.z / n);
}

_FORCE_INLINE_ bool operator==(const Vector3i & a, const Vector3i & b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

_FORCE_INLINE_ bool operator!=(const Vector3i & a, const Vector3i & b) {
    return a.x != b.x && a.y != b.y && a.z != b.z;
}

struct Vector3iHasher {
	static _FORCE_INLINE_ uint32_t hash(const Vector3i & v) {
		uint32_t hash = hash_djb2_one_32(v.x);
		hash = hash_djb2_one_32(v.y);
		return hash_djb2_one_32(v.z);
    }
};

#endif // VOXEL_VECTOR3I_H

