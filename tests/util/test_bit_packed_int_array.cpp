#include "test_bit_packed_int_array.h"
#include "../../util/containers/bit_packed_int_array.h"
#include "../../util/containers/palette_compressed_int_array.h"
#include "../../util/godot/core/random_pcg.h"
#include "../../util/testing/test_macros.h"

namespace zylann::tests {

void test_bit_packed_int_array_basic() {
	struct L {
		static void checked_set(BitPackedIntArray &bpa, const unsigned int i, const uint32_t in_v) {
			bpa.set(i, in_v);
			const uint32_t out_v = bpa.get(i);
			ZN_TEST_ASSERT(out_v == in_v);
		}
	};
	{
		BitPackedIntArray bpa(3, 2);
		L::checked_set(bpa, 0, 1);
		L::checked_set(bpa, 1, 2);
		L::checked_set(bpa, 2, 3);
		{
			const uint32_t v1 = bpa.get(0);
			const uint32_t v2 = bpa.get(1);
			const uint32_t v3 = bpa.get(2);
			ZN_TEST_ASSERT(v1 == 1);
			ZN_TEST_ASSERT(v2 == 2);
			ZN_TEST_ASSERT(v3 == 3);
		}
		{
			std::array<uint32_t, 3> a;
			bpa.read(Span<uint32_t>(a.data(), 3), 0);
			ZN_TEST_ASSERT(a[0] == 1);
			ZN_TEST_ASSERT(a[1] == 2);
			ZN_TEST_ASSERT(a[2] == 3);
		}

		bpa.resize_element_bits(8);
		L::checked_set(bpa, 0, 50);
		L::checked_set(bpa, 1, 100);
		L::checked_set(bpa, 2, 150);
		{
			const uint32_t v1 = bpa.get(0);
			const uint32_t v2 = bpa.get(1);
			const uint32_t v3 = bpa.get(2);
			ZN_TEST_ASSERT(v1 == 50);
			ZN_TEST_ASSERT(v2 == 100);
			ZN_TEST_ASSERT(v3 == 150);
		}
		{
			std::array<uint32_t, 3> a;
			bpa.read(Span<uint32_t>(a.data(), 3), 0);
			ZN_TEST_ASSERT(a[0] == 50);
			ZN_TEST_ASSERT(a[1] == 100);
			ZN_TEST_ASSERT(a[2] == 150);
		}
	}
	{
		std::array<uint32_t, 10> a;
		a[0] = 55;
		a[1] = 9455;
		a[2] = 0;
		a[3] = 4;
		a[4] = 256;
		a[5] = 8192;
		a[6] = 8191;
		a[7] = 58;
		a[8] = 22;
		a[9] = 3333;

		BitPackedIntArray bpa(10, 15);
		for (unsigned int i = 0; i < a.size(); ++i) {
			L::checked_set(bpa, i, a[i]);
		}
		for (unsigned int i = 0; i < a.size(); ++i) {
			const uint32_t v = bpa.get(i);
			ZN_TEST_ASSERT(v == a[i]);
		}

		std::array<uint32_t, 5> b;
		const uint32_t from = 3;
		bpa.read(Span<uint32_t>(b.data(), b.size()), from);
		for (unsigned int j = 0; j < b.size(); ++j) {
			const unsigned int i = from + j;
			const uint32_t v0 = a[i];
			const uint32_t v1 = b[j];
			const uint32_t v2 = bpa.get(i);
			ZN_TEST_ASSERT(v0 == v1);
			ZN_TEST_ASSERT(v1 == v2);
		}
	}
}

void pca_set(PaletteCompressedIntArray &pca, const unsigned int i, const uint32_t in_v) {
	// if (i == 15 && in_v == 16) {
	// 	ZN_PRINT_VERBOSE("A");
	// }
	pca.set(i, in_v);
	const uint32_t out_v = pca.get(i);
	ZN_TEST_ASSERT(in_v == out_v);
}

void test_palette_compressed_int_array_basic() {
	struct L {
		static void test(const uint32_t count, const uint32_t multiplier) {
			PaletteCompressedIntArray pca(count, 0);
			ZN_TEST_ASSERT(pca.get(0) == 0);

			for (unsigned int i = 0; i < count; ++i) {
				const uint32_t in_v = (i + 1) * multiplier;
				pca_set(pca, i, in_v);
			}
			for (unsigned int i = 0; i < count; ++i) {
				const uint32_t in_v = (i + 1) * multiplier;
				const uint32_t out_v = pca.get(i);
				ZN_TEST_ASSERT(out_v == in_v);
			}
		}
	};

	L::test(3, 1);
	L::test(3, 100);
	L::test(16, 1);
	L::test(16, 100);
	L::test(1000, 1);
	L::test(1000, 100);
}

void test_palette_compressed_int_array_compacting() {
	PaletteCompressedIntArray pca(64, 0);
	// Fill with a bunch of different values
	for (unsigned int i = 0; i < pca.size(); ++i) {
		const uint32_t in_v = i * 10;
		pca_set(pca, i, in_v);
	}
	// Fill back to 0 manually
	for (unsigned int i = 0; i < pca.size(); ++i) {
		pca_set(pca, i, 0);
	}
	// Set again a few values
	pca_set(pca, 10, 1);
	pca_set(pca, 20, 2);
	pca_set(pca, 30, 2);
	// That should compress the palette down
	pca.compact_palette();
	ZN_TEST_ASSERT(pca.get_palette().size() == 3);
	ZN_TEST_ASSERT(pca.get(10) == 1);
	ZN_TEST_ASSERT(pca.get(20) == 2);
	ZN_TEST_ASSERT(pca.get(30) == 2);
	for (unsigned int i = 0; i < pca.size(); ++i) {
		if (i != 10 && i != 20 && i != 30) {
			const uint32_t v = pca.get(i);
			ZN_TEST_ASSERT(v == 0);
		}
	}
}

void test_palette_compressed_int_array_fuzz() {
	struct L {
		static void check_equality(const PaletteCompressedIntArray &pca, const StdVector<uint32_t> &expected_values) {
			for (unsigned int i = 0; i < expected_values.size(); ++i) {
				const uint32_t in_v = expected_values[i];
				const uint32_t out_v = pca.get(i);
				ZN_TEST_ASSERT(in_v == out_v);
			}
		}
	};

	RandomPCG rng;
	rng.seed(131183);

	for (unsigned int iteration = 0; iteration < 1000; ++iteration) {
		const unsigned int count = 1 + rng.rand(6000);
		const unsigned int max_value = rng.rand(655360);

		StdVector<uint32_t> expected_values;
		expected_values.reserve(count);
		for (unsigned int i = 0; i < expected_values.size(); ++i) {
			expected_values.push_back(rng.rand(max_value));
		}

		PaletteCompressedIntArray pca(count, 0);

		// Do some random writes first
		const unsigned int random_sets_count = rng.rand(count * 4);
		for (unsigned int j = 0; j < random_sets_count; ++j) {
			const unsigned int i = rng.rand(pca.size());
			const uint32_t v = rng.rand(max_value);
			pca_set(pca, i, v);
		}

		// Set expected values
		for (unsigned int i = 0; i < expected_values.size(); ++i) {
			pca_set(pca, i, expected_values[i]);
		}

		L::check_equality(pca, expected_values);

		pca.compact_palette();
		L::check_equality(pca, expected_values);
	}
}

} // namespace zylann::tests
