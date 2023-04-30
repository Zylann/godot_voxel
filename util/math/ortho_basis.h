#ifndef ZN_ORTHO_BASIS_H
#define ZN_ORTHO_BASIS_H

#include "vector3i.h"
#include "vector3t.h"

namespace zylann::math {

// Orthogonal bases are useful to define 3D rotations that only use 90-degree steps for angles. Because there is only 24
// possible cases, they can often be encoded as a single byte. It can be recovered as a regular basis using a lookup
// table.

static const int ORTHOGONAL_BASIS_COUNT = 24;
static const int ORTHOGONAL_BASIS_IDENTITY_INDEX = 0;

typedef Vector3T<int8_t> Vector3i8;

// Basis where every axis is a unit vector pointing at either -X, +X, -Y, +Y, -Z or +Z.
struct OrthoBasis {
	// Axes
	Vector3i8 x;
	Vector3i8 y;
	Vector3i8 z;

	OrthoBasis(Vector3i8 p_x, Vector3i8 p_y, Vector3i8 p_z) : x(p_x), y(p_y), z(p_z) {}

	inline bool operator==(const OrthoBasis &other) const {
		return x == other.x && y == other.y && z == other.z;
	}

	inline void rotate_x_90_cw() {
		x = math::rotate_x_90_cw(x);
		y = math::rotate_x_90_cw(y);
		z = math::rotate_x_90_cw(z);
	}

	inline void rotate_x_90_ccw() {
		x = math::rotate_x_90_ccw(x);
		y = math::rotate_x_90_ccw(y);
		z = math::rotate_x_90_ccw(z);
	}

	inline void rotate_y_90_cw() {
		x = math::rotate_y_90_cw(x);
		y = math::rotate_y_90_cw(y);
		z = math::rotate_y_90_cw(z);
	}

	inline void rotate_y_90_ccw() {
		x = math::rotate_y_90_ccw(x);
		y = math::rotate_y_90_ccw(y);
		z = math::rotate_y_90_ccw(z);
	}

	inline void rotate_z_90_cw() {
		x = math::rotate_z_90_cw(x);
		y = math::rotate_z_90_cw(y);
		z = math::rotate_z_90_cw(z);
	}

	inline void rotate_z_90_ccw() {
		x = math::rotate_z_90_ccw(x);
		y = math::rotate_z_90_ccw(y);
		z = math::rotate_z_90_ccw(z);
	}

	void rotate_90(Vector3i8::Axis axis, bool clockwise) {
		zylann::math::rotate_90(Span<Vector3i8>(&x, 3), axis, clockwise);
	}
};

OrthoBasis get_ortho_basis_from_index(int i);
int get_index_from_ortho_basis(const OrthoBasis &b);

} // namespace zylann::math

#endif // ZN_ORTHO_BASIS_H
