#include "ortho_basis.h"

namespace zylann::math {

// We can obtain a list of all the bases by positionning the origin at 8 corners of a cube. In one corner, the basis
// axes may align with 3 edges of the cube. We can rotate the corresponding edges 3 times per corner (X->Y, Y->Z, Z->X).
// So we get 3 * 8 = 24 bases.
// Another way is to pick an axis, then rotate the basis to point that axis at each of the 6
// directions (we have to pick a convention for up and down), and then further rotate the basis 4 times around that
// axis, which gives 4 * 6 = 24 bases.

// With no particular convention, each rotation can be given a name from the 3 axes XYZ in the form:
// `+x+y+z`
// `-y+x+z`
// `-x-y+z`
// ...

// If -Z is taken as forward, and up and down directions take reference from a rotation around X from identity, then
// each rotation can be given a name as one axis and a counter-clockwise angle:
// `-z+0`
// `-z-90`
// `-z+180`
// ...
// However more than one name can fit a given basis, for example `-z-180` is the same as `-z+180`.
// So the axis notation may be used if a unique name is preferred, the angle would have to be forced positive,
// or the up axis would have to be specified instead of an angle (the X axis always being on the right).

// Values are taken from Godot's GridMap code. The bases don't seem to follow a particular order, but we can see the
// third axis is used as pivot to obtain groups of 4 bases from each of the 6 directions.
static const OrthoBasis g_ortho_bases[ORTHOGONAL_BASIS_COUNT] = {
	OrthoBasis(Vector3i8(1, 0, 0), Vector3i8(0, 1, 0), Vector3i8(0, 0, 1)), // -z+0
	OrthoBasis(Vector3i8(0, -1, 0), Vector3i8(1, 0, 0), Vector3i8(0, 0, 1)), // -z+270
	OrthoBasis(Vector3i8(-1, 0, 0), Vector3i8(0, -1, 0), Vector3i8(0, 0, 1)), // -z+180
	OrthoBasis(Vector3i8(0, 1, 0), Vector3i8(-1, 0, 0), Vector3i8(0, 0, 1)), // -z+90

	OrthoBasis(Vector3i8(1, 0, 0), Vector3i8(0, 0, -1), Vector3i8(0, 1, 0)), // -x+270
	OrthoBasis(Vector3i8(0, 0, 1), Vector3i8(1, 0, 0), Vector3i8(0, 1, 0)), //
	OrthoBasis(Vector3i8(-1, 0, 0), Vector3i8(0, 0, 1), Vector3i8(0, 1, 0)), //
	OrthoBasis(Vector3i8(0, 0, -1), Vector3i8(-1, 0, 0), Vector3i8(0, 1, 0)), //

	OrthoBasis(Vector3i8(1, 0, 0), Vector3i8(0, -1, 0), Vector3i8(0, 0, -1)), //
	OrthoBasis(Vector3i8(0, 1, 0), Vector3i8(1, 0, 0), Vector3i8(0, 0, -1)), //
	OrthoBasis(Vector3i8(-1, 0, 0), Vector3i8(0, 1, 0), Vector3i8(0, 0, -1)), //
	OrthoBasis(Vector3i8(0, -1, 0), Vector3i8(-1, 0, 0), Vector3i8(0, 0, -1)), //

	OrthoBasis(Vector3i8(1, 0, 0), Vector3i8(0, 0, 1), Vector3i8(0, -1, 0)), //
	OrthoBasis(Vector3i8(0, 0, -1), Vector3i8(1, 0, 0), Vector3i8(0, -1, 0)), //
	OrthoBasis(Vector3i8(-1, 0, 0), Vector3i8(0, 0, -1), Vector3i8(0, -1, 0)), //
	OrthoBasis(Vector3i8(0, 0, 1), Vector3i8(-1, 0, 0), Vector3i8(0, -1, 0)), //

	OrthoBasis(Vector3i8(0, 0, 1), Vector3i8(0, 1, 0), Vector3i8(-1, 0, 0)), //
	OrthoBasis(Vector3i8(0, -1, 0), Vector3i8(0, 0, 1), Vector3i8(-1, 0, 0)), //
	OrthoBasis(Vector3i8(0, 0, -1), Vector3i8(0, -1, 0), Vector3i8(-1, 0, 0)), //
	OrthoBasis(Vector3i8(0, 1, 0), Vector3i8(0, 0, -1), Vector3i8(-1, 0, 0)), //

	OrthoBasis(Vector3i8(0, 0, 1), Vector3i8(0, -1, 0), Vector3i8(1, 0, 0)), //
	OrthoBasis(Vector3i8(0, 1, 0), Vector3i8(0, 0, 1), Vector3i8(1, 0, 0)), //
	OrthoBasis(Vector3i8(0, 0, -1), Vector3i8(0, 1, 0), Vector3i8(1, 0, 0)), //
	OrthoBasis(Vector3i8(0, -1, 0), Vector3i8(0, 0, -1), Vector3i8(1, 0, 0)) //
};

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

} // namespace zylann::math
