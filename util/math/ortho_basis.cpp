#include "ortho_basis.h"

namespace zylann::math {

// We can obtain a list of all the bases by positionning the origin at 8 corners of a cube. In one corner, the basis
// axes may align with 3 edges of the cube. We can rotate the corresponding edges 3 times per corner (X->Y, Y->Z, Z->X).
// So we get 3 * 8 = 24 bases.
// Another way is to pick an axis, then rotate the basis to point that axis at each of the 6
// directions (we have to pick a convention for up and down), and then further rotate the basis 4 times around that
// axis, which gives 4 * 6 = 24 bases.

// Values are taken from Godot's GridMap code. Order is arbitrary, but must remain the same to match enum values.
// clang-format off
static const OrthoBasis g_ortho_bases[ORTHOGONAL_BASIS_COUNT] = {
	OrthoBasis(Vector3i( 1,  0,  0), Vector3i( 0,  1,  0), Vector3i( 0,  0,  1)), // identity
	OrthoBasis(Vector3i( 0, -1,  0), Vector3i( 1,  0,  0), Vector3i( 0,  0,  1)), //
	OrthoBasis(Vector3i(-1,  0,  0), Vector3i( 0, -1,  0), Vector3i( 0,  0,  1)), //
	OrthoBasis(Vector3i( 0,  1,  0), Vector3i(-1,  0,  0), Vector3i( 0,  0,  1)), //

	OrthoBasis(Vector3i( 1,  0,  0), Vector3i( 0,  0, -1), Vector3i( 0,  1,  0)), //
	OrthoBasis(Vector3i( 0,  0,  1), Vector3i( 1,  0,  0), Vector3i( 0,  1,  0)), //
	OrthoBasis(Vector3i(-1,  0,  0), Vector3i( 0,  0,  1), Vector3i( 0,  1,  0)), //
	OrthoBasis(Vector3i( 0,  0, -1), Vector3i(-1,  0,  0), Vector3i( 0,  1,  0)), //

	OrthoBasis(Vector3i( 1,  0,  0), Vector3i( 0, -1,  0), Vector3i( 0,  0, -1)), //
	OrthoBasis(Vector3i( 0,  1,  0), Vector3i( 1,  0,  0), Vector3i( 0,  0, -1)), //
	OrthoBasis(Vector3i(-1,  0,  0), Vector3i( 0,  1,  0), Vector3i( 0,  0, -1)), //
	OrthoBasis(Vector3i( 0, -1,  0), Vector3i(-1,  0,  0), Vector3i( 0,  0, -1)), //

	OrthoBasis(Vector3i( 1,  0,  0), Vector3i( 0,  0,  1), Vector3i( 0, -1,  0)), //
	OrthoBasis(Vector3i( 0,  0, -1), Vector3i( 1,  0,  0), Vector3i( 0, -1,  0)), //
	OrthoBasis(Vector3i(-1,  0,  0), Vector3i( 0,  0, -1), Vector3i( 0, -1,  0)), //
	OrthoBasis(Vector3i( 0,  0,  1), Vector3i(-1,  0,  0), Vector3i( 0, -1,  0)), //

	OrthoBasis(Vector3i( 0,  0,  1), Vector3i( 0,  1,  0), Vector3i(-1,  0,  0)), //
	OrthoBasis(Vector3i( 0, -1,  0), Vector3i( 0,  0,  1), Vector3i(-1,  0,  0)), //
	OrthoBasis(Vector3i( 0,  0, -1), Vector3i( 0, -1,  0), Vector3i(-1,  0,  0)), //
	OrthoBasis(Vector3i( 0,  1,  0), Vector3i( 0,  0, -1), Vector3i(-1,  0,  0)), //

	OrthoBasis(Vector3i( 0,  0,  1), Vector3i( 0, -1,  0), Vector3i( 1,  0,  0)), //
	OrthoBasis(Vector3i( 0,  1,  0), Vector3i( 0,  0,  1), Vector3i( 1,  0,  0)), //
	OrthoBasis(Vector3i( 0,  0, -1), Vector3i( 0,  1,  0), Vector3i( 1,  0,  0)), //
	OrthoBasis(Vector3i( 0, -1,  0), Vector3i( 0,  0, -1), Vector3i( 1,  0,  0)) //
};
// clang-format on

OrthoBasis get_ortho_basis_from_index(int i) {
	ZN_ASSERT(i >= 0 && i < ORTHOGONAL_BASIS_COUNT);
	return g_ortho_bases[i];
}

int get_index_from_ortho_basis(const OrthoBasis &b) {
	for (int i = 0; i < ORTHOGONAL_BASIS_COUNT; ++i) {
		const OrthoBasis &ob = g_ortho_bases[i];
		if (ob == b) {
			return i;
		}
	}
	return -1;
}

// clang-format off
const char *s_rotation_names[ORTHO_ROTATION_COUNT] = {
	"identity",
	"z_270",
	"z_180",
	"z_90",
	"x_270",
	"x_270_y_270",
	"x_270_y_180",
	"x_270_y_90",
	"z_180_y_180",
	"z_90_y_180",
	"y_180",
	"z_270_y_180",
	"x_90",
	"x_90_y_90",
	"x_90_y_180",
	"x_90_y_270",
	"y_270",
	"z_270_y_270",
	"z_180_y_270",
	"z_90_y_270",
	"z_180_y_90",
	"z_90_y_90",
	"y_90",
	"z_270_y_90",
};
// clang-format on

const char *ortho_rotation_to_string(int i) {
	ZN_ASSERT_RETURN_V(i >= 0 && i < ORTHO_ROTATION_COUNT, "<error>");
	return s_rotation_names[i];
}

OrthoBasis OrthoBasis::from_axis_turns(const Vector3i::Axis axis, const int turns) {
	// If turns are negative, do the positive equivalent
	const int mturns = turns >= 0 ? turns % 4 : 4 - ((-turns) % 4);
	if (mturns == 0) {
		return OrthoBasis();
	}
	// Clockwise with the rotation axis pointing at us
	switch (axis) {
		case Vector3i::AXIS_X:
			switch (mturns) {
				case 1:
					return { Vector3i(1, 0, 0), Vector3i(0, 0, -1), Vector3i(0, 1, 0) };
				case 2:
					return { Vector3i(1, 0, 0), Vector3i(0, -1, 0), Vector3i(0, 0, -1) };
				case 3:
					return { Vector3i(1, 0, 0), Vector3i(0, 0, 1), Vector3i(0, -1, 0) };
				default:
					break;
			}
			break;
		case Vector3i::AXIS_Y:
			switch (mturns) {
				case 1:
					return { Vector3i(0, 0, 1), Vector3i(0, 1, 0), Vector3i(-1, 0, 0) };
				case 2:
					return { Vector3i(-1, 0, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1) };
				case 3:
					return { Vector3i(0, 0, -1), Vector3i(0, 1, 0), Vector3i(1, 0, 0) };
				default:
					break;
			}
			break;
		case Vector3i::AXIS_Z:
			switch (mturns) {
				case 1:
					return { Vector3i(0, -1, 0), Vector3i(1, 0, 0), Vector3i(0, 0, 1) };
				case 2:
					return { Vector3i(-1, 0, 0), Vector3i(0, -1, 0), Vector3i(0, 0, 1) };
				case 3:
					return { Vector3i(0, 1, 0), Vector3i(-1, 0, 0), Vector3i(0, 0, 1) };
				default:
					break;
			}
			break;
		default:
			ZN_PRINT_ERROR("Invalid axis");
	}
	return OrthoBasis();
}

bool OrthoBasis::is_orthonormal() const {
	return Vector3iUtil::is_unit_vector(x) && //
			Vector3iUtil::is_unit_vector(y) && //
			Vector3iUtil::is_unit_vector(z) && //
			math::dot(x, y) == 0 && //
			math::dot(x, z) == 0 && //
			math::dot(y, z) == 0;
}

} // namespace zylann::math
