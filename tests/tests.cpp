#include "tests.h"
#include "../generators/graph/range_utility.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../storage/voxel_data_map.h"
#include "../util/island_finder.h"
#include "../util/math/box3i.h"
#include "test_octree.h"

#include <core/hash_map.h>
#include <core/print_string.h>

void test_box3i_intersects() {
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		ERR_FAIL_COND(a.intersects(b) == false);
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(1, 0, 0), Vector3i(1, 1, 1));
		ERR_FAIL_COND(a.intersects(b) == true);
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(2, 2, 2));
		Box3i b(Vector3i(1, 0, 0), Vector3i(2, 2, 2));
		ERR_FAIL_COND(a.intersects(b) == false);
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 0), Vector3i(1, 10, 1));
		ERR_FAIL_COND(a.intersects(b) == false);
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 1), Vector3i(1, 10, 1));
		ERR_FAIL_COND(a.intersects(b) == true);
	}
}

void test_box3i_for_inner_outline() {
	const Box3i box(-1, 2, 3, 8, 6, 5);

	HashMap<Vector3i, bool, Vector3iHasher> expected_coords;
	const Box3i inner_box = box.padded(-1);
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
	static const int channel = VoxelBufferInternal::CHANNEL_TYPE;

	VoxelBufferInternal buffer;
	buffer.create(32, 16, 32);
	buffer.fill(voxel_value, channel);

	VoxelDataMap map;
	map.create(4, 0);

	const Box3i box(Vector3i(10, 10, 10), buffer.get_size());

	map.paste(box.pos, buffer, (1 << channel), false, 0, true);

	// All voxels in the area must be as pasted
	const bool is_match = box.all_cells_match([&map](const Vector3i &pos) {
		return map.get_voxel(pos, channel) == voxel_value;
	});

	ERR_FAIL_COND(!is_match);

	// Check neighbor voxels to make sure they were not changed
	const Box3i padded_box = box.padded(1);
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
	static const int channel = VoxelBufferInternal::CHANNEL_TYPE;

	VoxelBufferInternal buffer;
	buffer.create(32, 16, 32);
	// Fill the inside of the buffer with a value, and outline it with another value, which we'll use as mask
	buffer.fill(masked_value, channel);
	for (int z = 1; z < buffer.get_size().z - 1; ++z) {
		for (int x = 1; x < buffer.get_size().x - 1; ++x) {
			for (int y = 1; y < buffer.get_size().y - 1; ++y) {
				buffer.set_voxel(voxel_value, x, y, z, channel);
			}
		}
	}

	VoxelDataMap map;
	map.create(4, 0);

	const Box3i box(Vector3i(10, 10, 10), buffer.get_size());

	map.paste(box.pos, buffer, (1 << channel), true, masked_value, true);

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
	static const int channel = VoxelBufferInternal::CHANNEL_TYPE;

	VoxelDataMap map;
	map.create(4, 0);

	Box3i box(10, 10, 10, 32, 16, 32);
	VoxelBufferInternal buffer;
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

	map.paste(box.pos, buffer, (1 << channel), true, default_value, true);

	VoxelBufferInternal buffer2;
	buffer2.create(box.size);

	map.copy(box.pos, buffer2, (1 << channel));

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

	ERR_FAIL_COND(!buffer.equals(buffer2));
}

void test_encode_weights_packed_u16() {
	FixedArray<uint8_t, 4> weights;
	// There is data loss of the 4 smaller bits in this encoding,
	// so to test this we may use values greater than 16.
	// There is a compromise in decoding, where we choose that only values multiple of 16 are bijective.
	weights[0] = 1 << 4;
	weights[1] = 5 << 4;
	weights[2] = 10 << 4;
	weights[3] = 15 << 4;
	const uint16_t encoded_weights = encode_weights_to_packed_u16(weights[0], weights[1], weights[2], weights[3]);
	FixedArray<uint8_t, 4> decoded_weights = decode_weights_from_packed_u16(encoded_weights);
	ERR_FAIL_COND(weights != decoded_weights);
}

void test_copy_3d_region_zxy() {
	struct L {
		static void compare(
				Span<const uint16_t> srcs, Vector3i src_size, Vector3i src_min, Vector3i src_max,
				Span<const uint16_t> dsts, Vector3i dst_size, Vector3i dst_min) {
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
	};
	// Sub-region
	{
		std::vector<uint16_t> src;
		std::vector<uint16_t> dst;
		const Vector3i src_size(8, 8, 8);
		const Vector3i dst_size(3, 4, 5);
		src.resize(src_size.volume(), 0);
		dst.resize(dst_size.volume(), 0);
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
		std::vector<uint16_t> src;
		std::vector<uint16_t> dst;
		const Vector3i src_size(3, 4, 5);
		const Vector3i dst_size(3, 4, 5);
		src.resize(src_size.volume(), 0);
		dst.resize(dst_size.volume(), 0);
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

void test_voxel_graph_generator_default_graph_compilation() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instance();
	generator->load_plane_preset();
	VoxelGraphRuntime::CompilationResult result = generator->compile();
	ERR_FAIL_COND_MSG(!result.success,
			String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
}

void test_voxel_graph_generator_texturing() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instance();

	// Plane centered on Y=0, angled 45 degrees, going up towards +X
	// When Y<0, weight0 must be 1 and weight1 must be 0.
	// When Y>0, weight0 must be 0 and weight1 must be 1.
	// When 0<Y<1, weight0 must transition from 1 to 0 and weight1 must transition from 0 to 1.

	/*
	 *        Clamp --- Sub1 --- Weight0
	 *       /      \
	 *  Z   Y        Weight1
	 *       \
	 *  X --- Sub0 --- Sdf
	 * 
	 */

	const uint32_t in_x = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_X, Vector2(0, 0));
	const uint32_t in_y = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Y, Vector2(0, 0));
	const uint32_t in_z = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Z, Vector2(0, 0));
	const uint32_t out_sdf = generator->create_node(VoxelGeneratorGraph::NODE_OUTPUT_SDF, Vector2(0, 0));
	const uint32_t n_clamp = generator->create_node(VoxelGeneratorGraph::NODE_CLAMP, Vector2(0, 0));
	const uint32_t n_sub0 = generator->create_node(VoxelGeneratorGraph::NODE_SUBTRACT, Vector2(0, 0));
	const uint32_t n_sub1 = generator->create_node(VoxelGeneratorGraph::NODE_SUBTRACT, Vector2(0, 0));
	const uint32_t out_weight0 = generator->create_node(VoxelGeneratorGraph::NODE_OUTPUT_WEIGHT, Vector2(0, 0));
	const uint32_t out_weight1 = generator->create_node(VoxelGeneratorGraph::NODE_OUTPUT_WEIGHT, Vector2(0, 0));

	generator->set_node_default_input(n_sub1, 0, 1.0);
	generator->set_node_param(n_clamp, 0, 0.0);
	generator->set_node_param(n_clamp, 1, 1.0);
	generator->set_node_param(out_weight0, 0, 0);
	generator->set_node_param(out_weight1, 0, 1);

	generator->add_connection(in_y, 0, n_sub0, 0);
	generator->add_connection(in_x, 0, n_sub0, 1);
	generator->add_connection(n_sub0, 0, out_sdf, 0);
	generator->add_connection(in_y, 0, n_clamp, 0);
	generator->add_connection(n_clamp, 0, n_sub1, 1);
	generator->add_connection(n_sub1, 0, out_weight0, 0);
	generator->add_connection(n_clamp, 0, out_weight1, 0);

	VoxelGraphRuntime::CompilationResult compilation_result = generator->compile();
	ERR_FAIL_COND_MSG(!compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));

	// Single value tests
	{
		const float sdf_must_be_in_air = generator->generate_single(Vector3i(-2, 0, 0));
		const float sdf_must_be_in_ground = generator->generate_single(Vector3i(2, 0, 0));
		ERR_FAIL_COND(sdf_must_be_in_air <= 0.f);
		ERR_FAIL_COND(sdf_must_be_in_ground >= 0.f);

		uint32_t out_weight0_buffer_index;
		uint32_t out_weight1_buffer_index;
		ERR_FAIL_COND(!generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight0, 0 }, out_weight0_buffer_index));
		ERR_FAIL_COND(!generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight1, 0 }, out_weight1_buffer_index));

		// Sample two points 1 unit below ground at to heights on the slope

		{
			const float sdf = generator->generate_single(Vector3i(-2, -3, 0));
			ERR_FAIL_COND(sdf >= 0.f);
			const VoxelGraphRuntime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const VoxelGraphRuntime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const VoxelGraphRuntime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ERR_FAIL_COND(out_weight0_buffer.size < 1);
			ERR_FAIL_COND(out_weight0_buffer.data == nullptr);
			ERR_FAIL_COND(out_weight0_buffer.data[0] < 1.f);

			ERR_FAIL_COND(out_weight1_buffer.size < 1);
			ERR_FAIL_COND(out_weight1_buffer.data == nullptr);
			ERR_FAIL_COND(out_weight1_buffer.data[0] > 0.f);
		}
		{
			const float sdf = generator->generate_single(Vector3i(2, 1, 0));
			ERR_FAIL_COND(sdf >= 0.f);
			const VoxelGraphRuntime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const VoxelGraphRuntime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const VoxelGraphRuntime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ERR_FAIL_COND(out_weight0_buffer.size < 1);
			ERR_FAIL_COND(out_weight0_buffer.data == nullptr);
			ERR_FAIL_COND(out_weight0_buffer.data[0] > 0.f);

			ERR_FAIL_COND(out_weight1_buffer.size < 1);
			ERR_FAIL_COND(out_weight1_buffer.data == nullptr);
			ERR_FAIL_COND(out_weight1_buffer.data[0] < 1.f);
		}
	}

	// Block tests
	{
		// packed U16 format decoding has a slightly lower maximum due to a compromise
		const uint8_t WEIGHT_MAX = 240;

		struct L {
			static void check_weights(VoxelBufferInternal &buffer, Vector3i pos,
					bool weight0_must_be_1, bool weight1_must_be_1) {
				const uint16_t encoded_indices = buffer.get_voxel(pos, VoxelBufferInternal::CHANNEL_INDICES);
				const uint16_t encoded_weights = buffer.get_voxel(pos, VoxelBufferInternal::CHANNEL_WEIGHTS);
				const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
				const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);
				for (unsigned int i = 0; i < indices.size(); ++i) {
					switch (indices[i]) {
						case 0:
							if (weight0_must_be_1) {
								ERR_FAIL_COND(weights[i] < WEIGHT_MAX);
							} else {
								ERR_FAIL_COND(weights[i] > 0);
							}
							break;
						case 1:
							if (weight1_must_be_1) {
								ERR_FAIL_COND(weights[i] < WEIGHT_MAX);
							} else {
								ERR_FAIL_COND(weights[i] > 0);
							}
							break;
						default:
							break;
					}
				}
			}

			static void do_block_tests(Ref<VoxelGeneratorGraph> generator) {
				ERR_FAIL_COND(generator.is_null());
				{
					// Block centered on origin
					VoxelBufferInternal buffer;
					buffer.create(Vector3i(16, 16, 16));

					VoxelBlockRequest request{ buffer, -buffer.get_size() / 2, 0 };
					generator->generate_block(request);

					L::check_weights(buffer, Vector3i(4, 3, 8), true, false);
					L::check_weights(buffer, Vector3i(12, 11, 8), false, true);
				}
				{
					// Two blocks: one above 0, the other below.
					// The point is to check possible bugs due to optimizations.

					// Below 0
					VoxelBufferInternal buffer0;
					{
						buffer0.create(Vector3i(16, 16, 16));
						VoxelBlockRequest request{ buffer0, Vector3i(0, -16, 0), 0 };
						generator->generate_block(request);
					}

					// Above 0
					VoxelBufferInternal buffer1;
					{
						buffer1.create(Vector3i(16, 16, 16));
						VoxelBlockRequest request{ buffer1, Vector3i(0, 0, 0), 0 };
						generator->generate_block(request);
					}

					L::check_weights(buffer0, Vector3i(8, 8, 8), true, false);
					L::check_weights(buffer1, Vector3i(8, 8, 8), false, true);
				}
			}
		};

		// Putting state on the stack because the debugger doesnt let me access it
		const VoxelGraphRuntime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

		// Try first without optimization
		generator->set_use_optimized_execution_map(false);
		L::do_block_tests(generator);
		// Try with optimization
		generator->set_use_optimized_execution_map(true);
		L::do_block_tests(generator);
	}
}

void test_island_finder() {
	const char *cdata =
			"X X X - X "
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
	ERR_FAIL_COND(grid_size.volume() != strlen(cdata) / 2);

	std::vector<int> grid;
	grid.resize(grid_size.volume());
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
	output.resize(grid_size.volume());
	unsigned int label_count;

	IslandFinder island_finder;
	island_finder.scan_3d(
			Box3i(Vector3i(), grid_size),
			[&grid, grid_size](Vector3i pos) {
				const unsigned int i = pos.get_zxy_index(grid_size);
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

	ERR_FAIL_COND(label_count != 3);
}

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

		unordered_remove_if(vec, [](int v) {
			return v == 0;
		});

		ERR_FAIL_COND(vec.size() != 3);
		ERR_FAIL_COND((
							  L::count(vec, 0) == 0 &&
							  L::count(vec, 1) == 1 &&
							  L::count(vec, 2) == 1 &&
							  L::count(vec, 3) == 1) == false);
	}
	// Remove one in middle
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) {
			return v == 2;
		});

		ERR_FAIL_COND(vec.size() != 3);
		ERR_FAIL_COND((
							  L::count(vec, 0) == 1 &&
							  L::count(vec, 1) == 1 &&
							  L::count(vec, 2) == 0 &&
							  L::count(vec, 3) == 1) == false);
	}
	// Remove one at end
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) {
			return v == 3;
		});

		ERR_FAIL_COND(vec.size() != 3);
		ERR_FAIL_COND((
							  L::count(vec, 0) == 1 &&
							  L::count(vec, 1) == 1 &&
							  L::count(vec, 2) == 1 &&
							  L::count(vec, 3) == 0) == false);
	}
	// Remove multiple
	{
		std::vector<int> vec;
		vec.push_back(0);
		vec.push_back(1);
		vec.push_back(2);
		vec.push_back(3);

		unordered_remove_if(vec, [](int v) {
			return v == 1 || v == 2;
		});

		ERR_FAIL_COND(vec.size() != 2);
		ERR_FAIL_COND((
							  L::count(vec, 0) == 1 &&
							  L::count(vec, 1) == 0 &&
							  L::count(vec, 2) == 0 &&
							  L::count(vec, 3) == 1) == false);
	}
	// Remove last
	{
		std::vector<int> vec;
		vec.push_back(0);

		unordered_remove_if(vec, [](int v) {
			return v == 0;
		});

		ERR_FAIL_COND(vec.size() != 0);
	}
}

void test_instance_data_serialization() {
	struct L {
		static VoxelInstanceBlockData::InstanceData create_instance(
				float x, float y, float z, float rotx, float roty, float rotz, float scale) {
			VoxelInstanceBlockData::InstanceData d;
			d.transform = Transform(
					Basis().rotated(Vector3(rotx, roty, rotz)).scaled(Vector3(scale, scale, scale)),
					Vector3(x, y, z));
			return d;
		}
	};

	// Create some example data
	VoxelInstanceBlockData src_data;
	{
		src_data.position_range = 30;
		{
			VoxelInstanceBlockData::LayerData layer;
			layer.id = 1;
			layer.scale_min = 1.f;
			layer.scale_max = 1.f;
			layer.instances.push_back(L::create_instance(0, 0, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(10, 0, 0, 3.14, 0, 0, 1));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, 3.14, 0, 1));
			layer.instances.push_back(L::create_instance(0, 0, 30, 0, 0, 3.14, 1));
			src_data.layers.push_back(layer);
		}
		{
			VoxelInstanceBlockData::LayerData layer;
			layer.id = 2;
			layer.scale_min = 1.f;
			layer.scale_max = 4.f;
			layer.instances.push_back(L::create_instance(0, 1, 0, 0, 0, 0, 1));
			layer.instances.push_back(L::create_instance(20, 1, 0, -2.14, 0, 0, 2));
			layer.instances.push_back(L::create_instance(0, 20, 0, 0, -2.14, 0, 3));
			layer.instances.push_back(L::create_instance(0, 1, 20, -1, 0, 2.14, 4));
			src_data.layers.push_back(layer);
		}
	}

	std::vector<uint8_t> serialized_data;

	ERR_FAIL_COND(!serialize_instance_block_data(src_data, serialized_data));

	VoxelInstanceBlockData dst_data;
	ERR_FAIL_COND(!deserialize_instance_block_data(dst_data, to_span_const(serialized_data)));

	// Compare blocks
	ERR_FAIL_COND(src_data.layers.size() != dst_data.layers.size());
	ERR_FAIL_COND(dst_data.position_range < 0.f);
	ERR_FAIL_COND(dst_data.position_range != src_data.position_range);

	const float distance_error = max(src_data.position_range, VoxelInstanceBlockData::POSITION_RANGE_MINIMUM) /
								 float(VoxelInstanceBlockData::POSITION_RESOLUTION);

	// Compare layers
	for (unsigned int layer_index = 0; layer_index < dst_data.layers.size(); ++layer_index) {
		const VoxelInstanceBlockData::LayerData &src_layer = src_data.layers[layer_index];
		const VoxelInstanceBlockData::LayerData &dst_layer = dst_data.layers[layer_index];

		ERR_FAIL_COND(src_layer.id != dst_layer.id);
		if (src_layer.scale_max - src_layer.scale_min < VoxelInstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) {
			ERR_FAIL_COND(src_layer.scale_min != dst_layer.scale_min);
		} else {
			ERR_FAIL_COND(src_layer.scale_min != dst_layer.scale_min);
			ERR_FAIL_COND(src_layer.scale_max != dst_layer.scale_max);
		}
		ERR_FAIL_COND(src_layer.instances.size() != dst_layer.instances.size());

		const float scale_error =
				max(src_layer.scale_max - src_layer.scale_min, VoxelInstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) /
				float(VoxelInstanceBlockData::SIMPLE_11B_V1_SCALE_RESOLUTION);

		const float rotation_error = 2.f / float(VoxelInstanceBlockData::SIMPLE_11B_V1_QUAT_RESOLUTION);

		// Compare instances
		for (unsigned int instance_index = 0; instance_index < src_layer.instances.size(); ++instance_index) {
			const VoxelInstanceBlockData::InstanceData &src_instance = src_layer.instances[instance_index];
			const VoxelInstanceBlockData::InstanceData &dst_instance = dst_layer.instances[instance_index];

			ERR_FAIL_COND(src_instance.transform.origin.distance_to(dst_instance.transform.origin) > distance_error);

			const Vector3 src_scale = src_instance.transform.basis.get_scale();
			const Vector3 dst_scale = dst_instance.transform.basis.get_scale();
			ERR_FAIL_COND(src_scale.distance_to(dst_scale) > scale_error);

			// Had to normalize here because Godot doesn't want to give you a Quat if the basis is scaled (even uniformly)
			const Quat src_rot = src_instance.transform.basis.orthonormalized().get_quat();
			const Quat dst_rot = dst_instance.transform.basis.orthonormalized().get_quat();
			const float rot_dx = Math::abs(src_rot.x - dst_rot.x);
			const float rot_dy = Math::abs(src_rot.y - dst_rot.y);
			const float rot_dz = Math::abs(src_rot.z - dst_rot.z);
			const float rot_dw = Math::abs(src_rot.w - dst_rot.w);
			ERR_FAIL_COND(rot_dx > rotation_error);
			ERR_FAIL_COND(rot_dy > rotation_error);
			ERR_FAIL_COND(rot_dz > rotation_error);
			ERR_FAIL_COND(rot_dw > rotation_error);
		}
	}
}

void test_transform_3d_array_zxy() {
	// YXZ
	int src_grid[] = {
		0, 1, 2, 3,
		4, 5, 6, 7,
		8, 9, 10, 11,

		12, 13, 14, 15,
		16, 17, 18, 19,
		20, 21, 22, 23
	};
	const Vector3i src_size(3, 4, 2);
	const int volume = src_size.volume();

	FixedArray<int, 24> dst_grid;
	ERR_FAIL_COND(dst_grid.size() != volume);

	{
		int expected_dst_grid[] = {
			0, 4, 8,
			1, 5, 9,
			2, 6, 10,
			3, 7, 11,

			12, 16, 20,
			13, 17, 21,
			14, 18, 22,
			15, 19, 23
		};
		const Vector3i expected_dst_size(4, 3, 2);
		IntBasis basis;
		basis.x = Vector3i(0, 1, 0);
		basis.y = Vector3i(1, 0, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size = transform_3d_array_zxy(
				Span<const int>(src_grid, 0, volume),
				to_span(dst_grid), src_size, basis);

		ERR_FAIL_COND(dst_size != expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ERR_FAIL_COND(dst_grid[i] != expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			3, 2, 1, 0,
			7, 6, 5, 4,
			11, 10, 9, 8,

			15, 14, 13, 12,
			19, 18, 17, 16,
			23, 22, 21, 20
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, 1);

		const Vector3i dst_size = transform_3d_array_zxy(
				Span<const int>(src_grid, 0, volume),
				to_span(dst_grid), src_size, basis);

		ERR_FAIL_COND(dst_size != expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ERR_FAIL_COND(dst_grid[i] != expected_dst_grid[i]);
		}
	}
	{
		int expected_dst_grid[] = {
			15, 14, 13, 12,
			19, 18, 17, 16,
			23, 22, 21, 20,

			3, 2, 1, 0,
			7, 6, 5, 4,
			11, 10, 9, 8
		};
		const Vector3i expected_dst_size(3, 4, 2);
		IntBasis basis;
		basis.x = Vector3i(1, 0, 0);
		basis.y = Vector3i(0, -1, 0);
		basis.z = Vector3i(0, 0, -1);

		const Vector3i dst_size = transform_3d_array_zxy(
				Span<const int>(src_grid, 0, volume),
				to_span(dst_grid), src_size, basis);

		ERR_FAIL_COND(dst_size != expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ERR_FAIL_COND(dst_grid[i] != expected_dst_grid[i]);
		}
	}
}

void test_get_curve_monotonic_sections() {
	// This one is a bit annoying to test because Curve has float precision issues stemming from the bake() function
	struct L {
		static bool is_equal_approx(float a, float b) {
			return Math::is_equal_approx(a, b, 2.f * CURVE_RANGE_MARGIN);
		}
	};
	{
		// One segment going up
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 1);
		ERR_FAIL_COND(sections[0].x_min != 0.f);
		ERR_FAIL_COND(sections[0].x_max != 1.f);
		ERR_FAIL_COND(sections[0].y_min != 0.f);
		ERR_FAIL_COND(sections[0].y_max != 1.f);
		{
			Interval yi = get_curve_range(**curve, sections, Interval(0.f, 1.f));
			ERR_FAIL_COND(!L::is_equal_approx(yi.min, 0.f));
			ERR_FAIL_COND(!L::is_equal_approx(yi.max, 1.f));
		}
		{
			Interval yi = get_curve_range(**curve, sections, Interval(-2.f, 2.f));
			ERR_FAIL_COND(!L::is_equal_approx(yi.min, 0.f));
			ERR_FAIL_COND(!L::is_equal_approx(yi.max, 1.f));
		}
		{
			Interval xi(0.2f, 0.8f);
			Interval yi = get_curve_range(**curve, sections, xi);
			Interval yi_expected(curve->interpolate_baked(xi.min), curve->interpolate_baked(xi.max));
			ERR_FAIL_COND(!L::is_equal_approx(yi.min, yi_expected.min));
			ERR_FAIL_COND(!L::is_equal_approx(yi.max, yi_expected.max));
		}
	}
	{
		// One flat segment
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 1);
		ERR_FAIL_COND(sections[0].x_min != 0.f);
		ERR_FAIL_COND(sections[0].x_max != 1.f);
		ERR_FAIL_COND(sections[0].y_min != 0.f);
		ERR_FAIL_COND(sections[0].y_max != 0.f);
	}
	{
		// Two segments: going up, then flat
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 1);
	}
	{
		// Two segments: flat, then up
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 1);
	}
	{
		// Three segments: flat, then up, then flat
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 0));
		curve->add_point(Vector2(0.6, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 1);
	}
	{
		// Three segments: up, down, up
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 1));
		curve->add_point(Vector2(0.6, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 3);
		ERR_FAIL_COND(sections[0].x_min != 0.f);
		ERR_FAIL_COND(sections[2].x_max != 1.f);
	}
	{
		// Two segments: going up, then down
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 2);
	}
	{
		// One segment, curved as a parabola going up then down
		Ref<Curve> curve;
		curve.instance();
		curve->add_point(Vector2(0, 0), 0.f, 1.f);
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ERR_FAIL_COND(sections.size() != 2);
		ERR_FAIL_COND(sections[0].x_min != 0.f);
		ERR_FAIL_COND(sections[0].y_max < 0.1f);
		ERR_FAIL_COND(sections[1].x_max != 1.f);
	}
}

void test_voxel_buffer_create() {
	// This test was a repro for a memory corruption crash.
	VoxelBufferInternal generated_voxels;
	generated_voxels.create(Vector3i(5, 5, 5));
	generated_voxels.set_voxel_f(-0.7f, 3, 3, 3, VoxelBufferInternal::CHANNEL_SDF);
	generated_voxels.create(Vector3i(16, 16, 18));
	// This was found to cause memory corruption at this point because channels got re-allocated using the new size,
	// but were filled using the old size, which was greater, and accessed out of bounds memory.
	// The old size was used because the `_size` member was assigned too late in the process.
	// The corruption did not cause a crash here, but somewhere random where malloc was used shortly after.
	generated_voxels.create(Vector3i(1, 16, 18));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VOXEL_TEST(fname)                                     \
	print_line(String("Running {0}").format(varray(#fname))); \
	fname()

void run_voxel_tests() {
	print_line("------------ Voxel tests begin -------------");

	VOXEL_TEST(test_box3i_intersects);
	VOXEL_TEST(test_box3i_for_inner_outline);
	VOXEL_TEST(test_voxel_data_map_paste_fill);
	VOXEL_TEST(test_voxel_data_map_paste_mask);
	VOXEL_TEST(test_voxel_data_map_copy);
	VOXEL_TEST(test_encode_weights_packed_u16);
	VOXEL_TEST(test_copy_3d_region_zxy);
	VOXEL_TEST(test_voxel_graph_generator_default_graph_compilation);
	VOXEL_TEST(test_voxel_graph_generator_texturing);
	VOXEL_TEST(test_island_finder);
	VOXEL_TEST(test_unordered_remove_if);
	VOXEL_TEST(test_instance_data_serialization);
	VOXEL_TEST(test_transform_3d_array_zxy);
	VOXEL_TEST(test_octree_update);
	VOXEL_TEST(test_octree_find_in_box);
	VOXEL_TEST(test_get_curve_monotonic_sections);
	VOXEL_TEST(test_voxel_buffer_create);

	print_line("------------ Voxel tests end -------------");
}
