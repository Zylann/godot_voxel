#include "test_math_funcs.h"
#include "../../util/math/funcs.h"
#include "../testing.h"

namespace zylann::tests {

void test_wrap() {
	struct L {
		static void test(int d) {
			int expected = 0;

			const int range_min = -d * 10;
			const int range_max = d * 10;

			for (int x = range_min; x < range_max; ++x) {
				const int result = math::wrap(x, d);

				ZN_TEST_ASSERT(result == expected);

				++expected;
				if (expected == d) {
					expected = 0;
				}
			}
		}
	};

	for (int d = 1; d < 10; ++d) {
		L::test(d);
	}
}

} // namespace zylann::tests
