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

	void rotate_90(Axis axis, bool clockwise) {
		zylann::math::rotate_90(Span<Vector3i8>(&x, 3), axis, clockwise);
	}
};

// With no particular convention, each rotation can be given a name from the 3 axes XYZ in the form:
// `+x+y+z`
// `-y+x+z`
// `-x-y+z`
// ...
//
// A slightly more intuitive naming can be used, by considering -Z as forward, Y as up, X as right, and all
// rotations counter-clockwise, preferring either of the following forms:
//
// identity
// y+A             (Y rotation)
// z+A             (roll around forward)
// x+90|270        (up or down)
// x+90|270, y+B   (up or down + Y rotation)
// z+A, y+B        (roll around forward + Y rotation)
//
// Note: half of rotations are those with roll around Z. In a Minecraft game, they are never used.
//
enum OrthoRotationID {
	ORTHO_ROTATION_IDENTITY = 0,

	ORTHO_ROTATION_Z_270, // 1 (roll 270)
	ORTHO_ROTATION_Z_180, // 2 (roll 180)
	ORTHO_ROTATION_Z_90, // 3 (roll 90)
	ORTHO_ROTATION_X_270, // 4 (look down)
	ORTHO_ROTATION_X_270_Y_270, // 5 (look down, turn right)
	ORTHO_ROTATION_X_270_Y_180, // 6 (look down, turn around Y 180)
	ORTHO_ROTATION_X_270_Y_90, // 7 (look down, turn left)
	ORTHO_ROTATION_Z_180_Y_180, // 8 (roll 180, turn around Y 180)
	ORTHO_ROTATION_Z_90_Y_180, // 9 (roll 90, turn around Y 180)
	ORTHO_ROTATION_Y_180, // 10  (turn around Y 180)
	ORTHO_ROTATION_Z_270_Y_180, // 11  (roll 270, turn around Y 180)
	ORTHO_ROTATION_X_90, // 12 (look up)
	ORTHO_ROTATION_X_90_Y_90, // 13 (look up, turn left)
	ORTHO_ROTATION_X_90_Y_180, // 14 (look up, turn around Y 180)
	ORTHO_ROTATION_X_90_Y_270, // 15 (look up, turn right)
	ORTHO_ROTATION_Y_270, // 16 (turn right)
	ORTHO_ROTATION_Z_270_Y_270, // 17 (roll 270, turn right)
	ORTHO_ROTATION_Z_180_Y_270, // 18 (roll 180, turn right)
	ORTHO_ROTATION_Z_90_Y_270, // 19 (roll 90, turn right)
	ORTHO_ROTATION_Z_180_Y_90, // 20 (roll 180, turn left)
	ORTHO_ROTATION_Z_90_Y_90, // 21 (roll 90, turn left)
	ORTHO_ROTATION_Y_90, // 22 (turn left)
	ORTHO_ROTATION_Z_270_Y_90, // 23 (roll 270, turn left)

	ORTHO_ROTATION_COUNT,

	// Alternative
	//
	// IDENTITY (FORWARD)
	// FORWARD_ROLL_270
	// FORWARD_ROLL_180
	// FORWARD_ROLL_90
	// DOWN
	// DOWN_ROLL_270
	// DOWN_ROLL_180
	// DOWN_ROLL_90
	// BACK_ROLL_180
	// BACK_ROLL_90
	// BACK
	// BACK_ROLL_270
	// UP
	// UP_ROLL_90
	// UP_ROLL_180
	// UP_ROLL_270
	// RIGHT
	// RIGHT_ROLL_270
	// RIGHT_ROLL_180
	// RIGHT_ROLL_90
	// LEFT_ROLL_180
	// LEFT_ROLL_90
	// LEFT
	// LEFT_ROLL_270
};

OrthoBasis get_ortho_basis_from_index(int i);
int get_index_from_ortho_basis(const OrthoBasis &b);
const char *ortho_rotation_to_string(int i);

} // namespace zylann::math

#endif // ZN_ORTHO_BASIS_H
