#include "test_box3i.h"
#include "../../util/containers/std_unordered_map.h"
#include "../../util/math/box3i.h"
#include "../testing.h"

namespace zylann::tests {

void test_box3i_intersects() {
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		ZN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(1, 0, 0), Vector3i(1, 1, 1));
		ZN_TEST_ASSERT(a.intersects(b) == false);
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(2, 2, 2));
		Box3i b(Vector3i(1, 0, 0), Vector3i(2, 2, 2));
		ZN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 0), Vector3i(1, 10, 1));
		ZN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 1), Vector3i(1, 10, 1));
		ZN_TEST_ASSERT(a.intersects(b) == false);
	}
}

void test_box3i_for_inner_outline() {
	const Box3i box(-1, 2, 3, 8, 6, 5);

	StdUnorderedMap<Vector3i, bool> expected_coords;
	const Box3i inner_box = box.padded(-1);
	box.for_each_cell([&expected_coords, inner_box](Vector3i pos) {
		if (!inner_box.contains(pos)) {
			expected_coords.insert({ pos, false });
		}
	});

	box.for_inner_outline([&expected_coords](Vector3i pos) {
		auto it = expected_coords.find(pos);
		ZN_TEST_ASSERT_MSG(it != expected_coords.end(), "Position must be on the inner outline");
		ZN_TEST_ASSERT_MSG(it->second == false, "Position must be unique");
		it->second = true;
	});

	for (auto it = expected_coords.begin(); it != expected_coords.end(); ++it) {
		const bool v = it->second;
		ZN_TEST_ASSERT_MSG(v, "All expected coordinates must have been found");
	}
}

} // namespace zylann::tests
