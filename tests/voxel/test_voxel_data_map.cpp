#include "test_voxel_data_map.h"
#include "../../storage/voxel_buffer.h"
#include "../../storage/voxel_data_map.h"
#include "../../util/testing/test_macros.h"

namespace zylann::voxel::tests {

void test_voxel_data_map_paste_fill() {
	static const int voxel_value = 1;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	buffer.create(32, 16, 32);
	buffer.fill(voxel_value, channel);

	VoxelDataMap map;
	map.create(0);

	const Box3i box(Vector3i(10, 10, 10), buffer.get_size());

	map.paste(box.position, buffer, (1 << channel), true, true);

	// All voxels in the area must be as pasted
	const bool is_match = box.all_cells_match([&map](const Vector3i &pos) { //
		return map.get_voxel(pos, channel) == voxel_value;
	});

	ZN_TEST_ASSERT(is_match);

	// Check neighbor voxels to make sure they were not changed
	const Box3i padded_box = box.padded(1);
	bool outside_is_ok = true;
	padded_box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ZN_TEST_ASSERT(outside_is_ok);
}

void test_voxel_data_map_paste_mask() {
	static const int voxel_value = 1;
	static const int masked_value = 2;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	buffer.create(32, 16, 32);
	// Fill the inside of the buffer with a value, and outline it with another value, which we'll use as mask
	buffer.fill(masked_value, channel);
	buffer.fill_area(voxel_value, Vector3i(1, 1, 1), buffer.get_size() - Vector3i(1, 1, 1), channel);

	VoxelDataMap map;
	map.create(0);

	const Box3i box(Vector3i(10, 10, 10), buffer.get_size());

	map.paste_masked(
			box.position,
			buffer,
			(1 << channel),
			true,
			channel,
			masked_value,
			false,
			0,
			Span<const int32_t>(),
			true,
			true
	);

	// All voxels in the area must be as pasted. Ignoring the outline.
	const bool is_match = box.padded(-1).all_cells_match([&map](const Vector3i &pos) { //
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

	ZN_TEST_ASSERT(is_match);

	// Now check the outline voxels, they should be the same as before
	bool outside_is_ok = true;
	box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ZN_TEST_ASSERT(outside_is_ok);
}

void test_voxel_data_map_paste_dst_mask() {
	VoxelDataMap map;

	const Box3i box = Box3i::from_min_max(Vector3i(-10, -5, -10), Vector3i(10, 5, 10));
	static constexpr VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_TYPE;

	box.for_each_cell([&map, box](const Vector3i pos) { //
		const int v = pos.y - box.position.y;
		map.set_voxel(v, pos, channel);
	});

	VoxelBuffer vb(VoxelBuffer::ALLOCATOR_DEFAULT);
	vb.create(box.size);
	vb.fill(100, channel);

	const std::array<int32_t, 4> writable_values{ 0, 2, 5, 6 };
	map.paste_masked(
			box.position, vb, (1 << channel), true, channel, 999, true, channel, to_span(writable_values), false, true
	);

	box.for_each_cell([&map, box, &vb, writable_values](const Vector3i pos) {
		const int original_v = pos.y - box.position.y;
		const bool writable = contains(to_span(writable_values), original_v);
		const Vector3i rpos = pos - box.position;
		const int vbv = vb.get_voxel(rpos, channel);
		const int expected_v = writable ? vbv : original_v;
		const int found_v = map.get_voxel(pos, channel);
		ZN_TEST_ASSERT(found_v == expected_v);
	});
}

void test_voxel_data_map_copy() {
	static const int voxel_value = 1;
	static const int default_value = 0;
	static const int channel = VoxelBuffer::CHANNEL_TYPE;

	VoxelDataMap map;
	map.create(0);

	Box3i box(10, 10, 10, 32, 16, 32);
	VoxelBuffer buffer(VoxelBuffer::ALLOCATOR_DEFAULT);
	buffer.create(box.size);

	// Fill the inside of the buffer with a value, and leave outline to zero,
	// so our buffer isn't just uniform
	for (int z = 1; z < buffer.get_size().z - 1; ++z) {
		for (int x = 1; x < buffer.get_size().x - 1; ++x) {
			for (int y = 1; y < buffer.get_size().y - 1; ++y) {
				buffer.set_voxel(voxel_value, x, y, z, channel);
			}
		}
	}

	map.paste_masked(
			box.position,
			buffer,
			(1 << channel),
			true,
			channel,
			default_value,
			false,
			0,
			Span<const int32_t>(),
			true,
			true
	);

	VoxelBuffer buffer2(VoxelBuffer::ALLOCATOR_DEFAULT);
	buffer2.create(box.size);

	map.copy(box.position, buffer2, (1 << channel), true);

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

	ZN_TEST_ASSERT(buffer.equals(buffer2));
}

} // namespace zylann::voxel::tests
