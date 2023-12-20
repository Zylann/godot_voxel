#include "test_container_funcs.h"
#include "../../util/containers/container_funcs.h"
#include "../testing.h"

namespace zylann::tests {

void test_unordered_remove_if() {
	struct L {
		static unsigned int count(const std::vector<int> &vec, int v) {
			unsigned int n = 0;
			for (size_t i = 0; i < vec.size(); ++i) {
				if (vec[i] == v) {
					++n;
				}
			}
			return n;
		}
	};
	// Remove one at beginning
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 0 && L::count(vec, 1) == 1 && L::count(vec, 2) == 1 && L::count(vec, 3) == 1);
	}
	// Remove one in middle
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 2; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 1 && L::count(vec, 2) == 0 && L::count(vec, 3) == 1);
	}
	// Remove one at end
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 3; });

		ZN_TEST_ASSERT(vec.size() == 3);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 1 && L::count(vec, 2) == 1 && L::count(vec, 3) == 0);
	}
	// Remove multiple
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) { return v == 1 || v == 2; });

		ZN_TEST_ASSERT(vec.size() == 2);
		ZN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 0 && L::count(vec, 2) == 0 && L::count(vec, 3) == 1);
	}
	// Remove last
	{
		std::vector<int> vec;
		vec.push_back(0);

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZN_TEST_ASSERT(vec.size() == 0);
	}
}

} // namespace zylann::tests
