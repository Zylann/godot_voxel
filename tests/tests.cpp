#include "tests.h"
#include "../terrain/voxel_data_map.h"
#include "../util/math/rect3i.h"

#include <core/hash_map.h>
#include <core/print_string.h>

void test_rect3i_for_inner_outline() {
	const Rect3i box(-1, 2, 3, 8, 6, 5);

	HashMap<Vector3i, bool, Vector3iHasher> expected_coords;
	const Rect3i inner_box = box.padded(-1);
	box.for_each_cell([&expected_coords, inner_box](Vector3i pos) {
		if (!inner_box.contains(pos)) {
			expected_coords.set(pos, false);
		}
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

void test_voxel_data_map_paste_fill() {
	static const int voxel_value = 1;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	Ref<VoxelBuffer> buffer;
	buffer.instance();
	buffer->create(32, 16, 32);
	buffer->fill(voxel_value, channel);

	VoxelDataMap map;
	map.create(4, 0);

	const Rect3i box(Vector3i(10, 10, 10), buffer->get_size());

	map.paste(box.pos, **buffer, (1 << channel), std::numeric_limits<uint64_t>::max(), true);

	// All voxels in the area must be as pasted
	const bool is_match = box.all_cells_match([&map](const Vector3i &pos) {
		return map.get_voxel(pos, channel) == voxel_value;
	});

	ERR_FAIL_COND(!is_match);

	// Check neighbor voxels to make sure they were not changed
	const Rect3i padded_box = box.padded(1);
	bool outside_is_ok = true;
	padded_box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ERR_FAIL_COND(!outside_is_ok);
}

void test_voxel_data_map_paste_mask() {
	static const int voxel_value = 1;
	static const int masked_value = 2;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	Ref<VoxelBuffer> buffer;
	buffer.instance();
	buffer->create(32, 16, 32);
	// Fill the inside of the buffer with a value, and outline it with another value, which we'll use as mask
	buffer->fill(masked_value, channel);
	for (int z = 1; z < buffer->get_size().z - 1; ++z) {
		for (int x = 1; x < buffer->get_size().x - 1; ++x) {
			for (int y = 1; y < buffer->get_size().y - 1; ++y) {
				buffer->set_voxel(voxel_value, x, y, z, channel);
			}
		}
	}

	VoxelDataMap map;
	map.create(4, 0);

	const Rect3i box(Vector3i(10, 10, 10), buffer->get_size());

	map.paste(box.pos, **buffer, (1 << channel), masked_value, true);

	// All voxels in the area must be as pasted. Ignoring the outline.
	const bool is_match = box.padded(-1).all_cells_match([&map](const Vector3i &pos) {
		return map.get_voxel(pos, channel) == voxel_value;
	});

	/*for (int y = 0; y < buffer->get_size().y; ++y) {
		String line = String("y={0} | ").format(varray(y));
		for (int x = 0; x < buffer->get_size().x; ++x) {
			const int v = buffer->get_voxel(Vector3i(x, y, box.pos.z + 5), channel);
			if (v == default_value) {
				line += "- ";
			} else if (v == voxel_value) {
				line += "O ";
			} else if (v == masked_value) {
				line += "M ";
			}
		}
		print_line(line);
	}

	for (int y = 0; y < 64; ++y) {
		String line = String("y={0} | ").format(varray(y));
		for (int x = 0; x < 64; ++x) {
			const int v = map.get_voxel(Vector3i(x, y, box.pos.z + 5), channel);
			if (v == default_value) {
				line += "- ";
			} else if (v == voxel_value) {
				line += "O ";
			} else if (v == masked_value) {
				line += "M ";
			}
		}
		print_line(line);
	}*/

	ERR_FAIL_COND(!is_match);

	// Now check the outline voxels, they should be the same as before
	bool outside_is_ok = true;
	box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ERR_FAIL_COND(!outside_is_ok);
}

void test_voxel_data_map_copy() {
	static const int voxel_value = 1;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	VoxelDataMap map;
	map.create(4, 0);

	Rect3i box(10, 10, 10, 32, 16, 32);
	Ref<VoxelBuffer> buffer;
	buffer.instance();
	buffer->create(box.size);

	// Fill the inside of the buffer with a value, and leave outline to zero,
	// so our buffer isn't just uniform
	for (int z = 1; z < buffer->get_size().z - 1; ++z) {
		for (int x = 1; x < buffer->get_size().x - 1; ++x) {
			for (int y = 1; y < buffer->get_size().y - 1; ++y) {
				buffer->set_voxel(voxel_value, x, y, z, channel);
			}
		}
	}

	map.paste(box.pos, **buffer, (1 << channel), default_value, true);

	Ref<VoxelBuffer> buffer2;
	buffer2.instance();
	buffer2->create(box.size);

	map.copy(box.pos, **buffer2, (1 << channel));

	// for (int y = 0; y < buffer2->get_size().y; ++y) {
	// 	String line = String("y={0} | ").format(varray(y));
	// 	for (int x = 0; x < buffer2->get_size().x; ++x) {
	// 		const int v = buffer2->get_voxel(Vector3i(x, y, 5), channel);
	// 		if (v == default_value) {
	// 			line += "- ";
	// 		} else if (v == voxel_value) {
	// 			line += "O ";
	// 		} else {
	// 			line += "X ";
	// 		}
	// 	}
	// 	print_line(line);
	// }

	ERR_FAIL_COND(!buffer->equals(**buffer2));
}

void test_encode_weights_packed_u16() {
	FixedArray<uint8_t, 4> weights;
	// There is data loss of the 4 smaller bits in this encoding,
	// so to test this we may use values greater than 16
	weights[0] = 1 << 4;
	weights[1] = 5 << 4;
	weights[2] = 10 << 4;
	weights[3] = 15 << 4;
	const uint16_t encoded_weights = encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
	FixedArray<uint8_t, 4> decoded_weights = decode_weights_from_packed_u16(encoded_weights);
	ERR_FAIL_COND(weights != decoded_weights);
}

void test_copy_3d_region_zxy() {
	std::vector<uint16_t> src;
	std::vector<uint16_t> dst;
	const Vector3i src_size(8, 8, 8);
	const Vector3i dst_size(3, 4, 5);
	src.resize(src_size.volume(), 0);
	dst.resize(src_size.volume(), 0);
	for (unsigned int i = 0; i < src.size(); ++i) {
		src[i] = i;
	}

	ArraySlice<const uint16_t> srcs = to_slice_const(src);
	ArraySlice<uint16_t> dsts = to_slice(dst);
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

	Vector3i pos;
	for (pos.z = src_min.z; pos.z < src_max.z; ++pos.z) {
		for (pos.x = src_min.x; pos.x < src_max.x; ++pos.x) {
			for (pos.y = src_min.y; pos.y < src_max.y; ++pos.y) {
				const uint16_t srcv = srcs[pos.get_zxy_index(src_size)];
				const uint16_t dstv = dsts[(pos - src_min + dst_min).get_zxy_index(dst_size)];
				ERR_FAIL_COND(srcv != dstv);
			}
		}
	}
}

#define VOXEL_TEST(fname)                                     \
	print_line(String("Running {0}").format(varray(#fname))); \
	fname()

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	VOXEL_TEST(test_rect3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_copy);
	VOXEL_TEST(test_encode_weights_packed_u16);
	VOXEL_TEST(test_copy_3d_region_zxy);

	print_line("------------ Voxel tests end -------------");
}
