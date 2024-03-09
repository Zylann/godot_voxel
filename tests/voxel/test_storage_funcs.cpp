#include "test_storage_funcs.h"
#include "../../storage/funcs.h"
#include "../../util/containers/std_vector.h"
#include "../testing.h"

namespace zylann::voxel::tests {

void test_encode_weights_packed_u16() {
	FixedArray<uint8_t, 4> weights;
	// There is data loss of the 4 smaller bits in this encoding,
	// so to test this we may use values greater than 16.
	// There is a compromise in decoding, where we choose that only values multiple of 16 are bijective.
	weights[0] = 1 << 4;
	weights[1] = 5 << 4;
	weights[2] = 10 << 4;
	weights[3] = 15 << 4;
	const uint16_t encoded_weights = encode_weights_to_packed_u16_lossy(weights[0], weights[1], weights[2], weights[3]);
	FixedArray<uint8_t, 4> decoded_weights = decode_weights_from_packed_u16(encoded_weights);
	ZN_TEST_ASSERT(weights == decoded_weights);
}

void test_copy_3d_region_zxy() {
	struct L {
		static void compare(Span<const uint16_t> srcs, Vector3i src_size, Vector3i src_min, Vector3i src_max,
				Span<const uint16_t> dsts, Vector3i dst_size, Vector3i dst_min) {
			Vector3i pos;
			for (pos.z = src_min.z; pos.z < src_max.z; ++pos.z) {
				for (pos.x = src_min.x; pos.x < src_max.x; ++pos.x) {
					for (pos.y = src_min.y; pos.y < src_max.y; ++pos.y) {
						const uint16_t srcv = srcs[Vector3iUtil::get_zxy_index(pos, src_size)];
						const uint16_t dstv = dsts[Vector3iUtil::get_zxy_index(pos - src_min + dst_min, dst_size)];
						ZN_TEST_ASSERT(srcv == dstv);
					}
				}
			}
		}
	};
	// Sub-region
	{
		StdVector<uint16_t> src;
		StdVector<uint16_t> dst;
		const Vector3i src_size(8, 8, 8);
		const Vector3i dst_size(3, 4, 5);
		src.resize(Vector3iUtil::get_volume(src_size), 0);
		dst.resize(Vector3iUtil::get_volume(dst_size), 0);
		for (unsigned int i = 0; i < src.size(); ++i) {
			src[i] = i;
		}

		Span<const uint16_t> srcs = to_span_const(src);
		Span<uint16_t> dsts = to_span(dst);
		const Vector3i dst_min(0, 0, 0);
		const Vector3i src_min(2, 1, 0);
		const Vector3i src_max(5, 4, 3);
		copy_3d_region_zxy(dsts, dst_size, dst_min, srcs, src_size, src_min, src_max);

		/*for (pos.y = src_min.y; pos.y < src_max.y; ++pos.y) {
		String s;
		for (pos.x = src_min.x; pos.x < src_max.x; ++pos.x) {
			const uint16_t v = srcs[pos.get_zxy_index(src_size)];
			if (v < 10) {
				s += String("{0}   ").format(varray(v));
			} else if (v < 100) {
				s += String("{0}  ").format(varray(v));
			} else {
				s += String("{0} ").format(varray(v));
			}
		}
		print_line(s);
	}
	print_line("----");
	const Vector3i dst_max = dst_min + (src_max - src_min);
	pos = Vector3i();
	for (pos.y = dst_min.y; pos.y < dst_max.y; ++pos.y) {
		String s;
		for (pos.x = dst_min.x; pos.x < dst_max.x; ++pos.x) {
			const uint16_t v = dsts[pos.get_zxy_index(dst_size)];
			if (v < 10) {
				s += String("{0}   ").format(varray(v));
			} else if (v < 100) {
				s += String("{0}  ").format(varray(v));
			} else {
				s += String("{0} ").format(varray(v));
			}
		}
		print_line(s);
	}*/

		L::compare(srcs, src_size, src_min, src_max, to_span_const(dsts), dst_size, dst_min);
	}
	// Same size, full region
	{
		StdVector<uint16_t> src;
		StdVector<uint16_t> dst;
		const Vector3i src_size(3, 4, 5);
		const Vector3i dst_size(3, 4, 5);
		src.resize(Vector3iUtil::get_volume(src_size), 0);
		dst.resize(Vector3iUtil::get_volume(dst_size), 0);
		for (unsigned int i = 0; i < src.size(); ++i) {
			src[i] = i;
		}

		Span<const uint16_t> srcs = to_span_const(src);
		Span<uint16_t> dsts = to_span(dst);
		const Vector3i dst_min(0, 0, 0);
		const Vector3i src_min(0, 0, 0);
		const Vector3i src_max = src_size;
		copy_3d_region_zxy(dsts, dst_size, dst_min, srcs, src_size, src_min, src_max);

		L::compare(srcs, src_size, src_min, src_max, to_span_const(dsts), dst_size, dst_min);
	}
}

void test_transform_3d_array_zxy() {
	// YXZ
	int src_grid[] = {
		0, 1, 2, 3, //
		4, 5, 6, 7, //
		8, 9, 10, 11, //

		12, 13, 14, 15, //
		16, 17, 18, 19, //
		20, 21, 22, 23 //
	};
	const Vector3i src_size(3, 4, 2);
	const unsigned int volume = Vector3iUtil::get_volume(src_size);

	FixedArray<int, 24> dst_grid;
	ZN_TEST_ASSERT(dst_grid.size() == volume);

	{
		int expected_dst_grid[] = {
			0, 4, 8, //
			1, 5, 9, //
			2, 6, 10, //
			3, 7, 11, //

			12, 16, 20, //
			13, 17, 21, //
			14, 18, 22, //
			15, 19, 23 //
		};
		const Vector3i expected_dst_size(4, 3, 2);
		IntBasis basis;
		basis.x = Vector3i(0, 1, 0);
		basis.y = Vector3i(1, 0, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			3, 2, 1, 0, //
			7, 6, 5, 4, //
			11, 10, 9, 8, //

			15, 14, 13, 12, //
			19, 18, 17, 16, //
			23, 22, 21, 20 //
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			15, 14, 13, 12, //
			19, 18, 17, 16, //
			23, 22, 21, 20, //

			3, 2, 1, 0, //
			7, 6, 5, 4, //
			11, 10, 9, 8 //
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, -1);

		const Vector3i dst_size =
				transform_3d_array_zxy(Span<const int>(src_grid, 0, volume), to_span(dst_grid), src_size, basis);

		ZN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
		}
	}
}

} // namespace zylann::voxel::tests
