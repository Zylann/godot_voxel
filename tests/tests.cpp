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

#define VOXEL_TEST(fname)                                     \
	print_line(String("Running {0}").format(varray(#fname))); \
	fname()

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	VOXEL_TEST(test_rect3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_copy);

	print_line("------------ Voxel tests end -------------");
}
