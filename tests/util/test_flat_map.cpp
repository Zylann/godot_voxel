#include "test_flat_map.h"
#include "../../util/containers/flat_map.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/random_pcg.h"
#include "../testing.h"

namespace zylann::tests {

void test_flat_map() {
	struct Value {
		int i;
		bool operator==(const Value &other) const {
			return i == other.i;
		}
	};
	typedef FlatMap<int, Value>::Pair Pair;

	StdVector<Pair> sorted_pairs;
	for (int i = 0; i < 100; ++i) {
		sorted_pairs.push_back(Pair{ i, Value{ 1000 * i } });
	}
	const int inexistent_key1 = 101;
	const int inexistent_key2 = -1;

	struct L {
		static bool validate_map(const FlatMap<int, Value> &map, const StdVector<Pair> &sorted_pairs) {
			ZN_TEST_ASSERT_V(sorted_pairs.size() == map.size(), false);
			for (size_t i = 0; i < sorted_pairs.size(); ++i) {
				const Pair expected_pair = sorted_pairs[i];
				ZN_TEST_ASSERT_V(map.has(expected_pair.key), false);
				ZN_TEST_ASSERT_V(map.find(expected_pair.key) != nullptr, false);
				const Value *value = map.find(expected_pair.key);
				ZN_TEST_ASSERT_V(value != nullptr, false);
				ZN_TEST_ASSERT_V(*value == expected_pair.value, false);
			}
			return true;
		}
	};

	StdVector<Pair> shuffled_pairs = sorted_pairs;
	RandomPCG rng;
	rng.seed(131183);
	for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
		size_t dst_i = rng.rand() % shuffled_pairs.size();
		const Pair temp = shuffled_pairs[dst_i];
		shuffled_pairs[dst_i] = shuffled_pairs[i];
		shuffled_pairs[i] = temp;
	}

	{
		// Insert pre-sorted pairs
		FlatMap<int, Value> map;
		for (size_t i = 0; i < sorted_pairs.size(); ++i) {
			const Pair pair = sorted_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs with duplicates
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZN_TEST_ASSERT(map.insert(pair.key, pair.value));
			ZN_TEST_ASSERT_MSG(!map.insert(pair.key, pair.value), "Inserting the key a second time should fail");
		}
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Init from collection
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Inexistent items
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZN_TEST_ASSERT(!map.has(inexistent_key1));
		ZN_TEST_ASSERT(!map.has(inexistent_key2));
	}
	{
		// Iteration
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		size_t i = 0;
		for (FlatMap<int, Value>::ConstIterator it = map.begin(); it != map.end(); ++it) {
			ZN_TEST_ASSERT(i < sorted_pairs.size());
			const Pair expected_pair = sorted_pairs[i];
			ZN_TEST_ASSERT(expected_pair.key == it->key);
			ZN_TEST_ASSERT(expected_pair.value == it->value);
			++i;
		}
	}
}

} // namespace zylann::tests
