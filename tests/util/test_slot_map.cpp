#include "test_slot_map.h"
#include "../../util/containers/slot_map.h"
#include "../testing.h"

namespace zylann::tests {

void test_slot_map() {
	SlotMap<int> map;

	const SlotMap<int>::Key key1 = map.add(1);
	const SlotMap<int>::Key key2 = map.add(2);
	const SlotMap<int>::Key key3 = map.add(3);

	ZN_TEST_ASSERT(key1 != key2 && key2 != key3);
	ZN_TEST_ASSERT(map.exists(key1));
	ZN_TEST_ASSERT(map.exists(key2));
	ZN_TEST_ASSERT(map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 3);

	map.remove(key2);
	ZN_TEST_ASSERT(map.exists(key1));
	ZN_TEST_ASSERT(!map.exists(key2));
	ZN_TEST_ASSERT(map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 2);

	const SlotMap<int>::Key key4 = map.add(4);
	ZN_TEST_ASSERT(key4 != key2);
	ZN_TEST_ASSERT(map.count() == 3);

	const int v1 = map.get(key1);
	const int v4 = map.get(key4);
	const int v3 = map.get(key3);
	ZN_TEST_ASSERT(v1 == 1);
	ZN_TEST_ASSERT(v4 == 4);
	ZN_TEST_ASSERT(v3 == 3);

	map.clear();
	ZN_TEST_ASSERT(!map.exists(key1));
	ZN_TEST_ASSERT(!map.exists(key4));
	ZN_TEST_ASSERT(!map.exists(key3));
	ZN_TEST_ASSERT(map.count() == 0);
}

} // namespace zylann::tests
