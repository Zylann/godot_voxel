#include "test_island_finder.h"
#include "../../util/island_finder.h"
#include "../testing.h"

namespace zylann::tests {

void test_island_finder() {
	const char *cdata = "X X X - X "
						"X X X - - "
						"X X X - - "
						"X X X - - "
						"X X X - - "
						//
						"- - - - - "
						"X X - - - "
						"X X - - - "
						"X X X X X "
						"X X - - X "
						//
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - - - "
						//
						"- - - - - "
						"- - - - - "
						"- - X - - "
						"- - X X - "
						"- - - - - "
						//
						"- - - - - "
						"- - - - - "
						"- - - - - "
						"- - - X - "
						"- - - - - "
			//
			;

	const Vector3i grid_size(5, 5, 5);
	ZN_TEST_ASSERT(Vector3iUtil::get_volume(grid_size) == strlen(cdata) / 2);

	std::vector<int> grid;
	grid.resize(Vector3iUtil::get_volume(grid_size));
	for (unsigned int i = 0; i < grid.size(); ++i) {
		const char c = cdata[i * 2];
		if (c == 'X') {
			grid[i] = 1;
		} else if (c == '-') {
			grid[i] = 0;
		} else {
			ERR_FAIL();
		}
	}

	std::vector<uint8_t> output;
	output.resize(Vector3iUtil::get_volume(grid_size));
	unsigned int label_count;

	IslandFinder island_finder;
	island_finder.scan_3d(
			Box3i(Vector3i(), grid_size),
			[&grid, grid_size](Vector3i pos) {
				const unsigned int i = Vector3iUtil::get_zxy_index(pos, grid_size);
				CRASH_COND(i >= grid.size());
				return grid[i] == 1;
			},
			to_span(output), &label_count);

	// unsigned int i = 0;
	// for (int z = 0; z < grid_size.z; ++z) {
	// 	for (int x = 0; x < grid_size.x; ++x) {
	// 		String s;
	// 		for (int y = 0; y < grid_size.y; ++y) {
	// 			s += String::num_int64(output[i++]);
	// 			s += " ";
	// 		}
	// 		print_line(s);
	// 	}
	// 	print_line("//");
	// }

	ZN_TEST_ASSERT(label_count == 3);
}

} // namespace zylann::tests
