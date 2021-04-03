#include "tests.h"
#include "../util/math/rect3i.h"

#include <core/hash_map.h>
#include <core/print_string.h>

void test_rect3i_for_inner_outline() {
	const Rect3i box(-1, 2, 3, 4, 3, 2);

	HashMap<Vector3i, bool, Vector3iHasher> expected_coords;
	const Rect3i inner_box = box.padded(-1);
	box.for_each_cell([&expected_coords, inner_box](Vector3i pos) {
		expected_coords.set(pos, false);
	});

	box.for_inner_outline([&expected_coords](Vector3i pos) {
		bool *b = expected_coords.getptr(pos);
		if (b == nullptr) {
			ERR_FAIL_MSG("Unexpected position");
		}
		if (*b) {
			ERR_FAIL_MSG("Duplicate position");
		}
		*b = true;
	});

	const Vector3i *key = nullptr;
	while ((key = expected_coords.next(key))) {
		bool v = expected_coords[*key];
		if (!v) {
			ERR_FAIL_MSG("Missing position");
		}
	}
}

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	test_rect3i_for_inner_outline();

	print_line("------------ Voxel tests end -------------");
}
