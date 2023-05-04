#ifndef ZN_ORTHO_BASIS_H
#define ZN_ORTHO_BASIS_H

#include "conv.h"
#include "vector3i.h"
#include "vector3t.h"

namespace zylann::math {

// Orthogonal bases are useful to define 3D rotations that only use 90-degree steps for angles. Because there is only 24
// possible cases, they can often be encoded as a single byte. It can be recovered as a regular basis using a lookup
// table.

static const int ORTHOGONAL_BASIS_COUNT = 24;
static const int ORTHOGONAL_BASIS_IDENTITY_INDEX = 0;

typedef Vector3T<int8_t> Vector3i8;

// TODO Not sure if I should just wrap Basis instead of rewriting it with 8-bit members...

// Basis where every axis is a unit vector pointing at either -X, +X, -Y, +Y, -Z or +Z, and every axis is perpendicular
// to each other.
// There is no loss of precision when operating such basis, and equality comparison can be used safely.
struct OrthoBasis {
	// Axes
	Vector3i8 x;
	Vector3i8 y;
	Vector3i8 z;

	OrthoBasis() : x(1, 0, 0), y(0, 1, 0), z(0, 0, 1) {}
	OrthoBasis(Vector3i8 p_x, Vector3i8 p_y, Vector3i8 p_z) : x(p_x), y(p_y), z(p_z) {}

	inline bool operator==(const OrthoBasis &other) const {
		return x == other.x && y == other.y && z == other.z;
	}

	inline void transpose() {
		// x A A
		// B x A
		// B B x
		// We only need to swap the As with the Bs across the diagonal.
		SWAP(x.y, y.x);
		SWAP(x.z, z.x);
		SWAP(y.z, z.y);
	}

	inline void invert() {
		// The inverse of an orthogonal matrix is its transposed.
		// https://math.stackexchange.com/questions/1936020/why-is-the-inverse-of-an-orthogonal-matrix-equal-to-its-transpose
		transpose();
	}

	inline OrthoBasis inverted() const {
		OrthoBasis b = *this;
		b.invert();
		return b;
	}

	inline Vector3i xform(const Vector3i p) const {
		return p.x * to_vec3i(x) + p.y * to_vec3i(y) + p.z * to_vec3i(z);
	}

	inline Vector3i8 xform(const Vector3i8 p) const {
		return p.x * x + p.y * y + p.z * z;
	}

	inline math::OrthoBasis operator*(const math::OrthoBasis other) const {
		return math::OrthoBasis(xform(other.x), xform(other.y), xform(other.z));
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
