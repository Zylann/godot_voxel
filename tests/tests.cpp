#include "tests.h"
#include "../edition/voxel_tool_terrain.h"
#include "../generators/graph/range_utility.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../meshers/blocky/voxel_blocky_library.h"
#include "../meshers/cubes/voxel_mesher_cubes.h"
#include "../storage/voxel_buffer_gd.h"
#include "../storage/voxel_data_map.h"
#include "../storage/voxel_metadata_variant.h"
#include "../streams/instance_data.h"
#include "../streams/region/region_file.h"
#include "../streams/region/voxel_stream_region_files.h"
#include "../streams/voxel_block_serializer.h"
#include "../streams/voxel_block_serializer_gd.h"
#include "../util/container_funcs.h"
#include "../util/expression_parser.h"
#include "../util/flat_map.h"
#include "../util/godot/funcs.h"
#include "../util/island_finder.h"
#include "../util/math/box3i.h"
#include "../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../util/string_funcs.h"
#include "../util/tasks/threaded_task_runner.h"
#include "test_octree.h"
#include "testing.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../util/noise/fast_noise_2.h"
#endif

#include <core/io/dir_access.h>
#include <core/io/stream_peer.h>
#include <core/string/print_string.h>
#include <core/templates/hash_map.h>
#include <modules/noise/fastnoise_lite.h>

namespace zylann::voxel::tests {

void test_box3i_intersects() {
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		ZYLANN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(1, 1, 1));
		Box3i b(Vector3i(1, 0, 0), Vector3i(1, 1, 1));
		ZYLANN_TEST_ASSERT(a.intersects(b) == false);
	}
	{
		Box3i a(Vector3i(0, 0, 0), Vector3i(2, 2, 2));
		Box3i b(Vector3i(1, 0, 0), Vector3i(2, 2, 2));
		ZYLANN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 0), Vector3i(1, 10, 1));
		ZYLANN_TEST_ASSERT(a.intersects(b));
	}
	{
		Box3i a(Vector3i(-5, 0, 0), Vector3i(10, 1, 1));
		Box3i b(Vector3i(0, -5, 1), Vector3i(1, 10, 1));
		ZYLANN_TEST_ASSERT(a.intersects(b) == false);
	}
}

void test_box3i_for_inner_outline() {
	const Box3i box(-1, 2, 3, 8, 6, 5);

	std::unordered_map<Vector3i, bool> expected_coords;
	const Box3i inner_box = box.padded(-1);
	box.for_each_cell([&expected_coords, inner_box](Vector3i pos) {
		if (!inner_box.contains(pos)) {
			expected_coords.insert({ pos, false });
		}
	});

	box.for_inner_outline([&expected_coords](Vector3i pos) {
		auto it = expected_coords.find(pos);
		ZYLANN_TEST_ASSERT_MSG(it != expected_coords.end(), "Position must be on the inner outline");
		ZYLANN_TEST_ASSERT_MSG(it->second == false, "Position must be unique");
		it->second = true;
	});

	for (auto it = expected_coords.begin(); it != expected_coords.end(); ++it) {
		const bool v = it->second;
		ZYLANN_TEST_ASSERT_MSG(v, "All expected coordinates must have been found");
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
	const bool is_match =
			box.all_cells_match([&map](const Vector3i &pos) { return map.get_voxel(pos, channel) == voxel_value; });

	ZYLANN_TEST_ASSERT(is_match);

	// Check neighbor voxels to make sure they were not changed
	const Box3i padded_box = box.padded(1);
	bool outside_is_ok = true;
	padded_box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ZYLANN_TEST_ASSERT(outside_is_ok);
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
	const bool is_match = box.padded(-1).all_cells_match(
			[&map](const Vector3i &pos) { return map.get_voxel(pos, channel) == voxel_value; });

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

	ZYLANN_TEST_ASSERT(is_match);

	// Now check the outline voxels, they should be the same as before
	bool outside_is_ok = true;
	box.for_inner_outline([&map, &outside_is_ok](const Vector3i &pos) {
		if (map.get_voxel(pos, channel) != default_value) {
			outside_is_ok = false;
		}
	});

	ZYLANN_TEST_ASSERT(outside_is_ok);
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

	ZYLANN_TEST_ASSERT(buffer.equals(buffer2));
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
	ZYLANN_TEST_ASSERT(weights == decoded_weights);
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
						ZYLANN_TEST_ASSERT(srcv == dstv);
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
		std::vector<uint16_t> src;
		std::vector<uint16_t> dst;
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

void test_voxel_graph_generator_default_graph_compilation() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	generator->load_plane_preset();
	VoxelGraphRuntime::CompilationResult result = generator->compile(false);
	ZYLANN_TEST_ASSERT_MSG(
			result.success, String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
}

void test_voxel_graph_generator_expressions() {
	{
		Ref<VoxelGeneratorGraph> generator;
		generator.instantiate();

		const uint32_t in_x = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_X, Vector2(0, 0));
		const uint32_t in_y = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Y, Vector2(0, 0));
		const uint32_t in_z = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Z, Vector2(0, 0));
		const uint32_t out_sdf = generator->create_node(VoxelGeneratorGraph::NODE_OUTPUT_SDF, Vector2(0, 0));
		const uint32_t n_expression = generator->create_node(VoxelGeneratorGraph::NODE_EXPRESSION, Vector2());

		generator->set_node_param(n_expression, 0, "0.1 * x + 0.2 * z + min(y, 0.5)");
		PackedStringArray var_names;
		var_names.push_back("x");
		var_names.push_back("y");
		var_names.push_back("z");
		generator->set_expression_node_inputs(n_expression, var_names);

		generator->add_connection(in_x, 0, n_expression, 0);
		generator->add_connection(in_y, 0, n_expression, 1);
		generator->add_connection(in_z, 0, n_expression, 2);
		generator->add_connection(n_expression, 0, out_sdf, 0);

		VoxelGraphRuntime::CompilationResult result = generator->compile(false);
		ZYLANN_TEST_ASSERT_MSG(result.success,
				String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
	}
	Ref<ZN_FastNoiseLite> zfnl;
	{
		Ref<VoxelGeneratorGraph> generator;
		generator.instantiate();

		/*                       SdfPreview
								/
			  X --- FastNoise2D
				\/              \
				/\               \
			  Z --- Noise2D ----- a+b+c --- OutputSDF
								 /
			  Y --- SdfPlane ----
		*/

		const uint32_t in_x = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_X, Vector2(0, 0));
		const uint32_t in_y = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Y, Vector2(0, 0));
		const uint32_t in_z = generator->create_node(VoxelGeneratorGraph::NODE_INPUT_Z, Vector2(0, 0));
		const uint32_t out_sdf = generator->create_node(VoxelGeneratorGraph::NODE_OUTPUT_SDF, Vector2(0, 0));
		const uint32_t n_fn2d = generator->create_node(VoxelGeneratorGraph::NODE_FAST_NOISE_2D, Vector2());
		const uint32_t n_n2d = generator->create_node(VoxelGeneratorGraph::NODE_NOISE_2D, Vector2());
		const uint32_t n_plane = generator->create_node(VoxelGeneratorGraph::NODE_SDF_PLANE, Vector2());
		const uint32_t n_expr = generator->create_node(VoxelGeneratorGraph::NODE_EXPRESSION, Vector2());
		const uint32_t n_preview = generator->create_node(VoxelGeneratorGraph::NODE_SDF_PREVIEW, Vector2());

		generator->set_node_param(n_expr, 0, "a+b+c");
		PackedStringArray var_names;
		var_names.push_back("a");
		var_names.push_back("b");
		var_names.push_back("c");
		generator->set_expression_node_inputs(n_expr, var_names);

		zfnl.instantiate();
		generator->set_node_param(n_fn2d, 0, zfnl);

		Ref<FastNoiseLite> fnl;
		fnl.instantiate();
		generator->set_node_param(n_n2d, 0, fnl);

		generator->add_connection(in_x, 0, n_fn2d, 0);
		generator->add_connection(in_x, 0, n_n2d, 0);
		generator->add_connection(in_z, 0, n_fn2d, 1);
		generator->add_connection(in_z, 0, n_n2d, 1);
		generator->add_connection(in_y, 0, n_plane, 0);
		generator->add_connection(n_fn2d, 0, n_expr, 0);
		generator->add_connection(n_fn2d, 0, n_preview, 0);
		generator->add_connection(n_n2d, 0, n_expr, 1);
		generator->add_connection(n_plane, 0, n_expr, 2);
		generator->add_connection(n_expr, 0, out_sdf, 0);

		VoxelGraphRuntime::CompilationResult result = generator->compile(true);
		ZYLANN_TEST_ASSERT_MSG(result.success,
				String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));

		generator->generate_single(Vector3i(1, 2, 3), VoxelBufferInternal::CHANNEL_SDF);

		std::vector<VoxelGeneratorGraph::NodeProfilingInfo> profiling_info;
		generator->debug_measure_microseconds_per_voxel(false, &profiling_info);
		ZYLANN_TEST_ASSERT(profiling_info.size() >= 4);
		for (const VoxelGeneratorGraph::NodeProfilingInfo &info : profiling_info) {
			ZYLANN_TEST_ASSERT(generator->has_node(info.node_id));
		}
	}
	ZYLANN_TEST_ASSERT(zfnl.is_valid());
	ZYLANN_TEST_ASSERT(zfnl->reference_get_count() == 1);
}

void test_voxel_graph_generator_texturing() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();

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
	const uint32_t n_clamp = generator->create_node(VoxelGeneratorGraph::NODE_CLAMP_C, Vector2(0, 0));
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

	VoxelGraphRuntime::CompilationResult compilation_result = generator->compile(false);
	ZYLANN_TEST_ASSERT_MSG(compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));

	// Single value tests
	{
		const float sdf_must_be_in_air =
				generator->generate_single(Vector3i(-2, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
		const float sdf_must_be_in_ground =
				generator->generate_single(Vector3i(2, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
		ZYLANN_TEST_ASSERT(sdf_must_be_in_air > 0.f);
		ZYLANN_TEST_ASSERT(sdf_must_be_in_ground < 0.f);

		uint32_t out_weight0_buffer_index;
		uint32_t out_weight1_buffer_index;
		ZYLANN_TEST_ASSERT(generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight0, 0 }, out_weight0_buffer_index));
		ZYLANN_TEST_ASSERT(generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight1, 0 }, out_weight1_buffer_index));

		// Sample two points 1 unit below ground at to heights on the slope

		{
			const float sdf = generator->generate_single(Vector3i(-2, -3, 0), VoxelBufferInternal::CHANNEL_SDF).f;
			ZYLANN_TEST_ASSERT(sdf < 0.f);
			const VoxelGraphRuntime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const VoxelGraphRuntime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const VoxelGraphRuntime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ZYLANN_TEST_ASSERT(out_weight0_buffer.size >= 1);
			ZYLANN_TEST_ASSERT(out_weight0_buffer.data != nullptr);
			ZYLANN_TEST_ASSERT(out_weight0_buffer.data[0] >= 1.f);

			ZYLANN_TEST_ASSERT(out_weight1_buffer.size >= 1);
			ZYLANN_TEST_ASSERT(out_weight1_buffer.data != nullptr);
			ZYLANN_TEST_ASSERT(out_weight1_buffer.data[0] <= 0.f);
		}
		{
			const float sdf = generator->generate_single(Vector3i(2, 1, 0), VoxelBufferInternal::CHANNEL_SDF).f;
			ZYLANN_TEST_ASSERT(sdf < 0.f);
			const VoxelGraphRuntime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const VoxelGraphRuntime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const VoxelGraphRuntime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ZYLANN_TEST_ASSERT(out_weight0_buffer.size >= 1);
			ZYLANN_TEST_ASSERT(out_weight0_buffer.data != nullptr);
			ZYLANN_TEST_ASSERT(out_weight0_buffer.data[0] <= 0.f);

			ZYLANN_TEST_ASSERT(out_weight1_buffer.size >= 1);
			ZYLANN_TEST_ASSERT(out_weight1_buffer.data != nullptr);
			ZYLANN_TEST_ASSERT(out_weight1_buffer.data[0] >= 1.f);
		}
	}

	// Block tests
	{
		// packed U16 format decoding has a slightly lower maximum due to a compromise
		const uint8_t WEIGHT_MAX = 240;

		struct L {
			static void check_weights(
					VoxelBufferInternal &buffer, Vector3i pos, bool weight0_must_be_1, bool weight1_must_be_1) {
				const uint16_t encoded_indices = buffer.get_voxel(pos, VoxelBufferInternal::CHANNEL_INDICES);
				const uint16_t encoded_weights = buffer.get_voxel(pos, VoxelBufferInternal::CHANNEL_WEIGHTS);
				const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
				const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);
				for (unsigned int i = 0; i < indices.size(); ++i) {
					switch (indices[i]) {
						case 0:
							if (weight0_must_be_1) {
								ZYLANN_TEST_ASSERT(weights[i] >= WEIGHT_MAX);
							} else {
								ZYLANN_TEST_ASSERT(weights[i] <= 0);
							}
							break;
						case 1:
							if (weight1_must_be_1) {
								ZYLANN_TEST_ASSERT(weights[i] >= WEIGHT_MAX);
							} else {
								ZYLANN_TEST_ASSERT(weights[i] <= 0);
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

					VoxelGenerator::VoxelQueryData query{ buffer, -buffer.get_size() / 2, 0 };
					generator->generate_block(query);

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
						VoxelGenerator::VoxelQueryData query{ buffer0, Vector3i(0, -16, 0), 0 };
						generator->generate_block(query);
					}

					// Above 0
					VoxelBufferInternal buffer1;
					{
						buffer1.create(Vector3i(16, 16, 16));
						VoxelGenerator::VoxelQueryData query{ buffer1, Vector3i(0, 0, 0), 0 };
						generator->generate_block(query);
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
	const char *cdata = "X X X - X "
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
	ZYLANN_TEST_ASSERT(Vector3iUtil::get_volume(grid_size) == strlen(cdata) / 2);

	std::vector<int> grid;
	grid.resize(Vector3iUtil::get_volume(grid_size));
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
	output.resize(Vector3iUtil::get_volume(grid_size));
	unsigned int label_count;

	IslandFinder island_finder;
	island_finder.scan_3d(
			Box3i(Vector3i(), grid_size),
			[&grid, grid_size](Vector3i pos) {
				const unsigned int i = Vector3iUtil::get_zxy_index(pos, grid_size);
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

	ZYLANN_TEST_ASSERT(label_count == 3);
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

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZYLANN_TEST_ASSERT(vec.size() == 3);
		ZYLANN_TEST_ASSERT(
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

		ZYLANN_TEST_ASSERT(vec.size() == 3);
		ZYLANN_TEST_ASSERT(
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

		ZYLANN_TEST_ASSERT(vec.size() == 3);
		ZYLANN_TEST_ASSERT(
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

		ZYLANN_TEST_ASSERT(vec.size() == 2);
		ZYLANN_TEST_ASSERT(
				L::count(vec, 0) == 1 && L::count(vec, 1) == 0 && L::count(vec, 2) == 0 && L::count(vec, 3) == 1);
	}
	// Remove last
	{
		std::vector<int> vec;
		vec.push_back(0);

		unordered_remove_if(vec, [](int v) { return v == 0; });

		ZYLANN_TEST_ASSERT(vec.size() == 0);
	}
}

void test_instance_data_serialization() {
	struct L {
		static InstanceBlockData::InstanceData create_instance(
				float x, float y, float z, float rotx, float roty, float rotz, float scale) {
			InstanceBlockData::InstanceData d;
			d.transform = Transform3D(
					Basis().rotated(Vector3(rotx, roty, rotz)).scaled(Vector3(scale, scale, scale)), Vector3(x, y, z));
			return d;
		}
	};

	// Create some example data
	InstanceBlockData src_data;
	{
		src_data.position_range = 30;
		{
			InstanceBlockData::LayerData layer;
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
			InstanceBlockData::LayerData layer;
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

	ZYLANN_TEST_ASSERT(serialize_instance_block_data(src_data, serialized_data));

	InstanceBlockData dst_data;
	ZYLANN_TEST_ASSERT(deserialize_instance_block_data(dst_data, to_span_const(serialized_data)));

	// Compare blocks
	ZYLANN_TEST_ASSERT(src_data.layers.size() == dst_data.layers.size());
	ZYLANN_TEST_ASSERT(dst_data.position_range >= 0.f);
	ZYLANN_TEST_ASSERT(dst_data.position_range == src_data.position_range);

	const float distance_error = math::max(src_data.position_range, InstanceBlockData::POSITION_RANGE_MINIMUM) /
			float(InstanceBlockData::POSITION_RESOLUTION);

	// Compare layers
	for (unsigned int layer_index = 0; layer_index < dst_data.layers.size(); ++layer_index) {
		const InstanceBlockData::LayerData &src_layer = src_data.layers[layer_index];
		const InstanceBlockData::LayerData &dst_layer = dst_data.layers[layer_index];

		ZYLANN_TEST_ASSERT(src_layer.id == dst_layer.id);
		if (src_layer.scale_max - src_layer.scale_min < InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) {
			ZYLANN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
		} else {
			ZYLANN_TEST_ASSERT(src_layer.scale_min == dst_layer.scale_min);
			ZYLANN_TEST_ASSERT(src_layer.scale_max == dst_layer.scale_max);
		}
		ZYLANN_TEST_ASSERT(src_layer.instances.size() == dst_layer.instances.size());

		const float scale_error = math::max(src_layer.scale_max - src_layer.scale_min,
										  InstanceBlockData::SIMPLE_11B_V1_SCALE_RANGE_MINIMUM) /
				float(InstanceBlockData::SIMPLE_11B_V1_SCALE_RESOLUTION);

		const float rotation_error = 2.f / float(InstanceBlockData::SIMPLE_11B_V1_QUAT_RESOLUTION);

		// Compare instances
		for (unsigned int instance_index = 0; instance_index < src_layer.instances.size(); ++instance_index) {
			const InstanceBlockData::InstanceData &src_instance = src_layer.instances[instance_index];
			const InstanceBlockData::InstanceData &dst_instance = dst_layer.instances[instance_index];

			ZYLANN_TEST_ASSERT(
					src_instance.transform.origin.distance_to(dst_instance.transform.origin) <= distance_error);

			const Vector3 src_scale = src_instance.transform.basis.get_scale();
			const Vector3 dst_scale = dst_instance.transform.basis.get_scale();
			ZYLANN_TEST_ASSERT(src_scale.distance_to(dst_scale) <= scale_error);

			// Had to normalize here because Godot doesn't want to give you a Quat if the basis is scaled (even
			// uniformly)
			const Quaternion src_rot = src_instance.transform.basis.orthonormalized().get_quaternion();
			const Quaternion dst_rot = dst_instance.transform.basis.orthonormalized().get_quaternion();
			const float rot_dx = Math::abs(src_rot.x - dst_rot.x);
			const float rot_dy = Math::abs(src_rot.y - dst_rot.y);
			const float rot_dz = Math::abs(src_rot.z - dst_rot.z);
			const float rot_dw = Math::abs(src_rot.w - dst_rot.w);
			ZYLANN_TEST_ASSERT(rot_dx <= rotation_error);
			ZYLANN_TEST_ASSERT(rot_dy <= rotation_error);
			ZYLANN_TEST_ASSERT(rot_dz <= rotation_error);
			ZYLANN_TEST_ASSERT(rot_dw <= rotation_error);
		}
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
	const int volume = Vector3iUtil::get_volume(src_size);

	FixedArray<int, 24> dst_grid;
	ZYLANN_TEST_ASSERT(dst_grid.size() == volume);

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

		ZYLANN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZYLANN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
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

		ZYLANN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZYLANN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
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

		ZYLANN_TEST_ASSERT(dst_size == expected_dst_size);

		for (unsigned int i = 0; i < volume; ++i) {
			ZYLANN_TEST_ASSERT(dst_grid[i] == expected_dst_grid[i]);
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
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 1);
		ZYLANN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZYLANN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[0].y_max == 1.f);
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(0.f, 1.f));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval yi = get_curve_range(**curve, sections, math::Interval(-2.f, 2.f));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.min, 0.f));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.max, 1.f));
		}
		{
			math::Interval xi(0.2f, 0.8f);
			math::Interval yi = get_curve_range(**curve, sections, xi);
			math::Interval yi_expected(curve->interpolate_baked(xi.min), curve->interpolate_baked(xi.max));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.min, yi_expected.min));
			ZYLANN_TEST_ASSERT(L::is_equal_approx(yi.max, yi_expected.max));
		}
	}
	{
		// One flat segment
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 1);
		ZYLANN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[0].x_max == 1.f);
		ZYLANN_TEST_ASSERT(sections[0].y_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[0].y_max == 0.f);
	}
	{
		// Two segments: going up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Two segments: flat, then up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: flat, then up, then flat
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 0));
		curve->add_point(Vector2(0.6, 1));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 1);
	}
	{
		// Three segments: up, down, up
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.3, 1));
		curve->add_point(Vector2(0.6, 0));
		curve->add_point(Vector2(1, 1));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 3);
		ZYLANN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[2].x_max == 1.f);
	}
	{
		// Two segments: going up, then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0));
		curve->add_point(Vector2(0.5, 1));
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 2);
	}
	{
		// One segment, curved as a parabola going up then down
		Ref<Curve> curve;
		curve.instantiate();
		curve->add_point(Vector2(0, 0), 0.f, 1.f);
		curve->add_point(Vector2(1, 0));
		std::vector<CurveMonotonicSection> sections;
		get_curve_monotonic_sections(**curve, sections);
		ZYLANN_TEST_ASSERT(sections.size() == 2);
		ZYLANN_TEST_ASSERT(sections[0].x_min == 0.f);
		ZYLANN_TEST_ASSERT(sections[0].y_max >= 0.1f);
		ZYLANN_TEST_ASSERT(sections[1].x_max == 1.f);
	}
}

void test_voxel_buffer_create() {
	// This test was a repro for a memory corruption crash. The point of this test is to check it doesn't crash,
	// so there is no particular conditions to check.
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

void test_block_serializer() {
	// Create an example buffer
	const Vector3i block_size(8, 9, 10);
	VoxelBufferInternal voxel_buffer;
	voxel_buffer.create(block_size);
	voxel_buffer.fill_area(42, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 0);
	voxel_buffer.fill_area(43, Vector3i(2, 3, 4), Vector3i(6, 6, 6), 0);
	voxel_buffer.fill_area(44, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 1);

	{
		// Serialize without compression wrapper
		BlockSerializer::SerializeResult result = BlockSerializer::serialize(voxel_buffer);
		ZYLANN_TEST_ASSERT(result.success);
		std::vector<uint8_t> data = result.data;

		ZYLANN_TEST_ASSERT(data.size() > 0);
		ZYLANN_TEST_ASSERT(data[0] == BlockSerializer::BLOCK_FORMAT_VERSION);

		// Deserialize
		VoxelBufferInternal deserialized_voxel_buffer;
		ZYLANN_TEST_ASSERT(BlockSerializer::deserialize(to_span_const(data), deserialized_voxel_buffer));

		// Must be equal
		ZYLANN_TEST_ASSERT(voxel_buffer.equals(deserialized_voxel_buffer));
	}
	{
		// Serialize
		BlockSerializer::SerializeResult result = BlockSerializer::serialize_and_compress(voxel_buffer);
		ZYLANN_TEST_ASSERT(result.success);
		std::vector<uint8_t> data = result.data;

		ZYLANN_TEST_ASSERT(data.size() > 0);

		// Deserialize
		VoxelBufferInternal deserialized_voxel_buffer;
		ZYLANN_TEST_ASSERT(BlockSerializer::decompress_and_deserialize(to_span_const(data), deserialized_voxel_buffer));

		// Must be equal
		ZYLANN_TEST_ASSERT(voxel_buffer.equals(deserialized_voxel_buffer));
	}
}

void test_block_serializer_stream_peer() {
	// Create an example buffer
	const Vector3i block_size(8, 9, 10);
	Ref<gd::VoxelBuffer> voxel_buffer;
	voxel_buffer.instantiate();
	voxel_buffer->create(block_size.x, block_size.y, block_size.z);
	voxel_buffer->fill_area(42, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 0);
	voxel_buffer->fill_area(43, Vector3i(2, 3, 4), Vector3i(6, 6, 6), 0);
	voxel_buffer->fill_area(44, Vector3i(1, 2, 3), Vector3i(5, 5, 5), 1);

	Ref<StreamPeerBuffer> peer;
	peer.instantiate();
	//peer->clear();

	Ref<gd::VoxelBlockSerializer> serializer;
	serializer.instantiate();
	const int size = serializer->serialize(peer, voxel_buffer, true);

	PackedByteArray data_array = peer->get_data_array();

	// Client

	Ref<gd::VoxelBuffer> voxel_buffer2;
	voxel_buffer2.instantiate();

	Ref<StreamPeerBuffer> peer2;
	peer2.instantiate();
	peer2->set_data_array(data_array);

	Ref<gd::VoxelBlockSerializer> serializer2;
	serializer2.instantiate();

	serializer2->deserialize(peer2, voxel_buffer2, size, true);

	ZYLANN_TEST_ASSERT(voxel_buffer2->get_buffer().equals(voxel_buffer->get_buffer()));
}

void test_region_file() {
	const int block_size_po2 = 4;
	const int block_size = 1 << block_size_po2;
	const char *region_file_name = "test_region_file.vxr";
	zylann::testing::TestDirectory test_dir;
	ZYLANN_TEST_ASSERT(test_dir.is_valid());
	String region_file_path = test_dir.get_path().plus_file(region_file_name);

	struct RandomBlockGenerator {
		RandomPCG rng;

		void generate(VoxelBufferInternal &buffer) {
			buffer.create(Vector3iUtil::create(block_size));
			buffer.set_channel_depth(0, VoxelBufferInternal::DEPTH_16_BIT);

			// Make a block with enough data to take some significant space even if compressed
			for (int z = 0; z < buffer.get_size().z; ++z) {
				for (int x = 0; x < buffer.get_size().x; ++x) {
					for (int y = 0; y < buffer.get_size().y; ++y) {
						buffer.set_voxel(rng.rand() % 256, x, y, z, 0);
					}
				}
			}
		}
	};

	RandomBlockGenerator generator;

	// Create a block of voxels
	VoxelBufferInternal voxel_buffer;
	generator.generate(voxel_buffer);

	{
		RegionFile region_file;

		// Configure region format
		RegionFormat region_format = region_file.get_format();
		region_format.block_size_po2 = block_size_po2;
		for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
			region_format.channel_depths[channel_index] = voxel_buffer.get_channel_depth(channel_index);
		}
		ZYLANN_TEST_ASSERT(region_file.set_format(region_format));

		// Open file
		const Error open_error = region_file.open(region_file_path, true);
		ZYLANN_TEST_ASSERT(open_error == OK);

		// Save block
		const Error save_error = region_file.save_block(Vector3i(1, 2, 3), voxel_buffer);
		ZYLANN_TEST_ASSERT(save_error == OK);

		// Read back
		VoxelBufferInternal loaded_voxel_buffer;
		const Error load_error = region_file.load_block(Vector3i(1, 2, 3), loaded_voxel_buffer);
		ZYLANN_TEST_ASSERT(load_error == OK);

		// Must be equal
		ZYLANN_TEST_ASSERT(voxel_buffer.equals(loaded_voxel_buffer));
	}
	// Load again but using a new region file object
	{
		RegionFile region_file;

		// Open file
		const Error open_error = region_file.open(region_file_path, false);
		ZYLANN_TEST_ASSERT(open_error == OK);

		// Read back
		VoxelBufferInternal loaded_voxel_buffer;
		const Error load_error = region_file.load_block(Vector3i(1, 2, 3), loaded_voxel_buffer);
		ZYLANN_TEST_ASSERT(load_error == OK);

		// Must be equal
		ZYLANN_TEST_ASSERT(voxel_buffer.equals(loaded_voxel_buffer));
	}
	// Save many blocks
	{
		RegionFile region_file;

		// Open file
		const Error open_error = region_file.open(region_file_path, false);
		ZYLANN_TEST_ASSERT(open_error == OK);

		RandomPCG rng;

		std::unordered_map<Vector3i, VoxelBufferInternal> buffers;
		const Vector3i region_size = region_file.get_format().region_size;

		for (int i = 0; i < 1000; ++i) {
			const Vector3i pos = Vector3i( //
					rng.rand() % uint32_t(region_size.x), //
					rng.rand() % uint32_t(region_size.y), //
					rng.rand() % uint32_t(region_size.z) //
			);
			generator.generate(voxel_buffer);

			// Save block
			const Error save_error = region_file.save_block(pos, voxel_buffer);
			ZYLANN_TEST_ASSERT(save_error == OK);

			// Note, the same position can occur twice, we just overwrite
			buffers[pos] = std::move(voxel_buffer);
		}

		// Read back
		for (auto it = buffers.begin(); it != buffers.end(); ++it) {
			VoxelBufferInternal loaded_voxel_buffer;
			const Error load_error = region_file.load_block(it->first, loaded_voxel_buffer);
			ZYLANN_TEST_ASSERT(load_error == OK);
			ZYLANN_TEST_ASSERT(it->second.equals(loaded_voxel_buffer));
		}

		const Error close_error = region_file.close();
		ZYLANN_TEST_ASSERT(close_error == OK);

		// Open file
		const Error open_error2 = region_file.open(region_file_path, false);
		ZYLANN_TEST_ASSERT(open_error2 == OK);

		// Read back again
		for (auto it = buffers.begin(); it != buffers.end(); ++it) {
			VoxelBufferInternal loaded_voxel_buffer;
			const Error load_error = region_file.load_block(it->first, loaded_voxel_buffer);
			ZYLANN_TEST_ASSERT(load_error == OK);
			ZYLANN_TEST_ASSERT(it->second.equals(loaded_voxel_buffer));
		}
	}
}

// Test based on an issue from `I am the Carl` on Discord. It should only not crash or cause errors.
void test_voxel_stream_region_files() {
	const int block_size_po2 = 4;
	const int block_size = 1 << block_size_po2;

	zylann::testing::TestDirectory test_dir;
	ZYLANN_TEST_ASSERT(test_dir.is_valid());

	Ref<VoxelStreamRegionFiles> stream;
	stream.instantiate();
	stream->set_block_size_po2(block_size_po2);
	stream->set_directory(test_dir.get_path());

	RandomPCG rng;

	for (int cycle = 0; cycle < 1000; ++cycle) {
		VoxelBufferInternal buffer;
		buffer.create(block_size, block_size, block_size);

		// Make a block with enough data to take some significant space even if compressed
		for (int z = 0; z < buffer.get_size().z; ++z) {
			for (int x = 0; x < buffer.get_size().x; ++x) {
				for (int y = 0; y < buffer.get_size().y; ++y) {
					buffer.set_voxel(rng.rand() % 256, x, y, z, 0);
				}
			}
		}

		// The position isn't a correct use because it's in voxels, not blocks, but it remains a case that should
		// not cause errors or crash. The same blocks will simply get written to several times.
		VoxelStream::VoxelQueryData q{ buffer, Vector3(cycle, 0, 0), 0, VoxelStream::RESULT_ERROR };
		stream->save_voxel_block(q);
	}
}

#ifdef VOXEL_ENABLE_FAST_NOISE_2

void test_fast_noise_2() {
	// Very basic test. The point is to make sure it doesn't crash, so there is no special condition to check.
	Ref<FastNoise2> noise;
	noise.instantiate();
	float nv = noise->get_noise_2d_single(Vector2(42, 666));
	print_line(String("SIMD level: {0}").format(varray(FastNoise2::get_simd_level_name(noise->get_simd_level()))));
	print_line(String("Noise: {0}").format(varray(nv)));
	Ref<Image> im;
	im.instantiate();
	im->create(256, 256, false, Image::FORMAT_RGB8);
	noise->generate_image(im, false);
	//im->save_png("zylann_test_fastnoise2.png");
}

#endif

void test_run_blocky_random_tick() {
	const Box3i voxel_box(Vector3i(-24, -23, -22), Vector3i(64, 40, 40));

	// Create library with tickable voxels
	Ref<VoxelBlockyLibrary> library;
	library.instantiate();
	library->set_voxel_count(3);
	library->create_voxel(0, "air");
	library->create_voxel(1, "non_tickable");
	const int TICKABLE_ID = 2;
	Ref<VoxelBlockyModel> tickable_voxel = library->create_voxel(TICKABLE_ID, "tickable");
	tickable_voxel->set_random_tickable(true);

	// Create test map
	VoxelDataMap map;
	map.create(constants::DEFAULT_BLOCK_SIZE_PO2, 0);
	{
		// All blocks of this map will be the same,
		// an interleaving of all block types
		VoxelBufferInternal model_buffer;
		model_buffer.create(Vector3iUtil::create(map.get_block_size()));
		for (int z = 0; z < model_buffer.get_size().z; ++z) {
			for (int x = 0; x < model_buffer.get_size().x; ++x) {
				for (int y = 0; y < model_buffer.get_size().y; ++y) {
					const int block_id = (x + y + z) % 3;
					model_buffer.set_voxel(block_id, x, y, z, VoxelBufferInternal::CHANNEL_TYPE);
				}
			}
		}

		const Box3i world_blocks_box(-4, -4, -4, 8, 8, 8);
		world_blocks_box.for_each_cell_zxy([&map, &model_buffer](Vector3i block_pos) {
			std::shared_ptr<VoxelBufferInternal> buffer = make_shared_instance<VoxelBufferInternal>();
			buffer->create(model_buffer.get_size());
			buffer->copy_from(model_buffer);
			map.set_block_buffer(block_pos, buffer, false);
		});
	}

	struct Callback {
		Box3i voxel_box;
		Box3i pick_box;
		bool first_pick = true;
		bool ok = true;

		Callback(Box3i p_voxel_box) : voxel_box(p_voxel_box) {}

		bool exec(Vector3i pos, int block_id) {
			if (ok) {
				ok = _exec(pos, block_id);
			}
			return ok;
		}

		inline bool _exec(Vector3i pos, int block_id) {
			ZYLANN_TEST_ASSERT_V(block_id == TICKABLE_ID, false);
			ZYLANN_TEST_ASSERT_V(voxel_box.contains(pos), false);
			if (first_pick) {
				first_pick = false;
				pick_box = Box3i(pos, Vector3i(1, 1, 1));
			} else {
				pick_box.merge_with(Box3i(pos, Vector3i(1, 1, 1)));
			}
			return true;
		}
	};

	Callback cb(voxel_box);

	Math::seed(131183);

	VoxelToolTerrain::run_blocky_random_tick_static(
			map, voxel_box, **library, 1000, 4, &cb, [](void *self, Vector3i pos, int64_t val) {
				Callback *cb = (Callback *)self;
				return cb->exec(pos, val);
			});

	ZYLANN_TEST_ASSERT(cb.ok);

	// Even though there is randomness, we expect to see at least one hit
	ZYLANN_TEST_ASSERT_MSG(!cb.first_pick, "At least one hit is expected, not none");

	// Check that the points were more or less uniformly sparsed within the provided box.
	// They should, because we populated the world with a checkerboard of tickable voxels.
	// There is randomness at play, so unfortunately we may have to use a margin or pick the right seed,
	// and we only check the enclosing area.
	const int error_margin = 0;
	for (int axis_index = 0; axis_index < Vector3iUtil::AXIS_COUNT; ++axis_index) {
		const int nd = cb.pick_box.pos[axis_index] - voxel_box.pos[axis_index];
		const int pd = cb.pick_box.pos[axis_index] + cb.pick_box.size[axis_index] -
				(voxel_box.pos[axis_index] + voxel_box.size[axis_index]);
		ZYLANN_TEST_ASSERT(Math::abs(nd) <= error_margin);
		ZYLANN_TEST_ASSERT(Math::abs(pd) <= error_margin);
	}
}

void test_flat_map() {
	struct Value {
		int i;
		bool operator==(const Value &other) const {
			return i == other.i;
		}
	};
	typedef FlatMap<int, Value>::Pair Pair;

	std::vector<Pair> sorted_pairs;
	for (int i = 0; i < 100; ++i) {
		sorted_pairs.push_back(Pair{ i, Value{ 1000 * i } });
	}
	const int inexistent_key1 = 101;
	const int inexistent_key2 = -1;

	struct L {
		static bool validate_map(const FlatMap<int, Value> &map, const std::vector<Pair> &sorted_pairs) {
			ZYLANN_TEST_ASSERT_V(sorted_pairs.size() == map.size(), false);
			for (size_t i = 0; i < sorted_pairs.size(); ++i) {
				const Pair expected_pair = sorted_pairs[i];
				ZYLANN_TEST_ASSERT_V(map.has(expected_pair.key), false);
				ZYLANN_TEST_ASSERT_V(map.find(expected_pair.key) != nullptr, false);
				const Value *value = map.find(expected_pair.key);
				ZYLANN_TEST_ASSERT_V(value != nullptr, false);
				ZYLANN_TEST_ASSERT_V(*value == expected_pair.value, false);
			}
			return true;
		}
	};

	std::vector<Pair> shuffled_pairs = sorted_pairs;
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
			ZYLANN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZYLANN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZYLANN_TEST_ASSERT(map.insert(pair.key, pair.value));
		}
		ZYLANN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Insert random pairs with duplicates
		FlatMap<int, Value> map;
		for (size_t i = 0; i < shuffled_pairs.size(); ++i) {
			const Pair pair = shuffled_pairs[i];
			ZYLANN_TEST_ASSERT(map.insert(pair.key, pair.value));
			ZYLANN_TEST_ASSERT_MSG(!map.insert(pair.key, pair.value), "Inserting the key a second time should fail");
		}
		ZYLANN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Init from collection
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZYLANN_TEST_ASSERT(L::validate_map(map, sorted_pairs));
	}
	{
		// Inexistent items
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		ZYLANN_TEST_ASSERT(!map.has(inexistent_key1));
		ZYLANN_TEST_ASSERT(!map.has(inexistent_key2));
	}
	{
		// Iteration
		FlatMap<int, Value> map;
		map.clear_and_insert(to_span(shuffled_pairs));
		size_t i = 0;
		for (FlatMap<int, Value>::ConstIterator it = map.begin(); it != map.end(); ++it) {
			ZYLANN_TEST_ASSERT(i < sorted_pairs.size());
			const Pair expected_pair = sorted_pairs[i];
			ZYLANN_TEST_ASSERT(expected_pair.key == it->key);
			ZYLANN_TEST_ASSERT(expected_pair.value == it->value);
			++i;
		}
	}
}

void test_expression_parser() {
	using namespace ExpressionParser;

	{
		Result result = parse("", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("   ", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("42", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 42.f));
	}
	{
		Result result = parse("()", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("((()))", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("42)", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_UNEXPECTED_TOKEN);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("(42)", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 42.f));
	}
	{
		Result result = parse("(", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_UNCLOSED_PARENTHESIS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("(666", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_UNCLOSED_PARENTHESIS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1+", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_MISSING_OPERAND_ARGUMENTS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("++", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_MISSING_OPERAND_ARGUMENTS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1 2 3", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_MULTIPLE_OPERANDS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("???", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_INVALID_TOKEN);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		Result result = parse("1+2-3*4/5", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 0.6f));
	}
	{
		Result result = parse("1*2-3/4+5", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 6.25f));
	}
	{
		Result result = parse("(5 - 3)^2 + 2.5/(4 + 6)", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 4.25f));
	}
	{
		/*
					-
				   / \
				  /   \
				 /     \
				*       -
			   / \     / \
			  4   ^   c   d
				 / \
				+   2
			   / \
			  a   b
		*/
		UniquePtr<VariableNode> node_a = make_unique_instance<VariableNode>("a");
		UniquePtr<VariableNode> node_b = make_unique_instance<VariableNode>("b");
		UniquePtr<OperatorNode> node_add =
				make_unique_instance<OperatorNode>(OperatorNode::ADD, std::move(node_a), std::move(node_b));
		UniquePtr<NumberNode> node_two = make_unique_instance<NumberNode>(2);
		UniquePtr<OperatorNode> node_power =
				make_unique_instance<OperatorNode>(OperatorNode::POWER, std::move(node_add), std::move(node_two));
		UniquePtr<NumberNode> node_four = make_unique_instance<NumberNode>(4);
		UniquePtr<OperatorNode> node_mul =
				make_unique_instance<OperatorNode>(OperatorNode::MULTIPLY, std::move(node_four), std::move(node_power));
		UniquePtr<VariableNode> node_c = make_unique_instance<VariableNode>("c");
		UniquePtr<VariableNode> node_d = make_unique_instance<VariableNode>("d");
		UniquePtr<OperatorNode> node_sub =
				make_unique_instance<OperatorNode>(OperatorNode::SUBTRACT, std::move(node_c), std::move(node_d));
		UniquePtr<OperatorNode> expected_root =
				make_unique_instance<OperatorNode>(OperatorNode::SUBTRACT, std::move(node_mul), std::move(node_sub));

		Result result = parse("4*(a+b)^2-(c-d)", Span<const Function>());
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		// {
		// 	const std::string s1 = tree_to_string(*expected_root, Span<const Function>());
		// 	print_line(String(s1.c_str()));
		// 	print_line("---");
		// 	const std::string s2 = tree_to_string(*result.root, Span<const Function>());
		// 	print_line(String(s2.c_str()));
		// }
		ZYLANN_TEST_ASSERT(is_tree_equal(*result.root, *expected_root, Span<const Function>()));
	}
	{
		FixedArray<Function, 2> functions;

		{
			Function f;
			f.name = "sqrt";
			f.id = 0;
			f.argument_count = 1;
			f.func = [](Span<const float> args) { //
				return Math::sqrt(args[0]);
			};
			functions[0] = f;
		}
		{
			Function f;
			f.name = "clamp";
			f.id = 1;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[1] = f;
		}

		Result result = parse("clamp(sqrt(20 + sqrt(25)), 1, 2.0 * 2.0)", to_span_const(functions));
		ZYLANN_TEST_ASSERT(result.error.id == ERROR_NONE);
		ZYLANN_TEST_ASSERT(result.root != nullptr);
		ZYLANN_TEST_ASSERT(result.root->type == Node::NUMBER);
		const NumberNode &nn = static_cast<NumberNode &>(*result.root);
		ZYLANN_TEST_ASSERT(Math::is_equal_approx(nn.value, 4.f));
	}
	{
		FixedArray<Function, 2> functions;

		const unsigned int F_SIN = 0;
		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "sin";
			f.id = F_SIN;
			f.argument_count = 1;
			f.func = [](Span<const float> args) { //
				return Math::sin(args[0]);
			};
			functions[0] = f;
		}
		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[1] = f;
		}

		Result result = parse("x+sin(y, clamp(z, 0, 1))", to_span_const(functions));

		ZYLANN_TEST_ASSERT(result.error.id == ERROR_TOO_MANY_ARGUMENTS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z,", to_span_const(functions));

		ZYLANN_TEST_ASSERT(result.error.id == ERROR_EXPECTED_ARGUMENT);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z)", to_span_const(functions));

		ZYLANN_TEST_ASSERT(result.error.id == ERROR_TOO_FEW_ARGUMENTS);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
	{
		FixedArray<Function, 1> functions;

		const unsigned int F_CLAMP = 1;

		{
			Function f;
			f.name = "clamp";
			f.id = F_CLAMP;
			f.argument_count = 3;
			f.func = [](Span<const float> args) { //
				return math::clamp(args[0], args[1], args[2]);
			};
			functions[0] = f;
		}

		Result result = parse("clamp(z,)", to_span_const(functions));

		ZYLANN_TEST_ASSERT(result.error.id == ERROR_EXPECTED_ARGUMENT);
		ZYLANN_TEST_ASSERT(result.root == nullptr);
	}
}

class CustomMetadataTest : public ICustomVoxelMetadata {
public:
	static const uint8_t ID = VoxelMetadata::TYPE_CUSTOM_BEGIN + 10;

	uint8_t a;
	uint8_t b;
	uint8_t c;

	size_t get_serialized_size() const override {
		// Note, `sizeof(CustomMetadataTest)` gives 16 here. Probably because of vtable
		return 3;
	}

	size_t serialize(Span<uint8_t> dst) const override {
		dst[0] = a;
		dst[1] = b;
		dst[2] = c;
		return get_serialized_size();
	}

	bool deserialize(Span<const uint8_t> src, uint64_t &out_read_size) override {
		a = src[0];
		b = src[1];
		c = src[2];
		out_read_size = get_serialized_size();
		return true;
	}

	virtual ICustomVoxelMetadata *duplicate() {
		CustomMetadataTest *d = ZN_NEW(CustomMetadataTest);
		*d = *this;
		return d;
	}

	bool operator==(const CustomMetadataTest &other) const {
		return a == other.a && b == other.b && c == other.c;
	}
};

void test_voxel_buffer_metadata() {
	// Basic get and set
	{
		VoxelBufferInternal vb;
		vb.create(10, 10, 10);

		VoxelMetadata *meta = vb.get_or_create_voxel_metadata(Vector3i(1, 2, 3));
		ZYLANN_TEST_ASSERT(meta != nullptr);
		meta->set_u64(1234567890);

		const VoxelMetadata *meta2 = vb.get_voxel_metadata(Vector3i(1, 2, 3));
		ZYLANN_TEST_ASSERT(meta2 != nullptr);
		ZYLANN_TEST_ASSERT(meta2->get_type() == meta->get_type());
		ZYLANN_TEST_ASSERT(meta2->get_u64() == meta->get_u64());
	}
	// Serialization
	{
		VoxelBufferInternal vb;
		vb.create(10, 10, 10);

		{
			VoxelMetadata *meta0 = vb.get_or_create_voxel_metadata(Vector3i(1, 2, 3));
			ZYLANN_TEST_ASSERT(meta0 != nullptr);
			meta0->set_u64(1234567890);
		}

		{
			VoxelMetadata *meta1 = vb.get_or_create_voxel_metadata(Vector3i(4, 5, 6));
			ZYLANN_TEST_ASSERT(meta1 != nullptr);
			meta1->clear();
		}

		struct RemoveTypeOnExit {
			~RemoveTypeOnExit() {
				VoxelMetadataFactory::get_singleton().remove_constructor(CustomMetadataTest::ID);
			}
		};
		RemoveTypeOnExit rmtype;
		VoxelMetadataFactory::get_singleton().add_constructor_by_type<CustomMetadataTest>(CustomMetadataTest::ID);
		{
			VoxelMetadata *meta2 = vb.get_or_create_voxel_metadata(Vector3i(7, 8, 9));
			ZYLANN_TEST_ASSERT(meta2 != nullptr);
			CustomMetadataTest *custom = ZN_NEW(CustomMetadataTest);
			custom->a = 10;
			custom->b = 20;
			custom->c = 30;
			meta2->set_custom(CustomMetadataTest::ID, custom);
		}

		BlockSerializer::SerializeResult sresult = BlockSerializer::serialize(vb);
		ZYLANN_TEST_ASSERT(sresult.success);
		std::vector<uint8_t> bytes = sresult.data;

		VoxelBufferInternal rvb;
		ZYLANN_TEST_ASSERT(BlockSerializer::deserialize(to_span(bytes), rvb));

		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb_meta_map = vb.get_voxel_metadata();
		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &rvb_meta_map = rvb.get_voxel_metadata();

		ZYLANN_TEST_ASSERT(vb_meta_map.size() == rvb_meta_map.size());

		for (auto it = vb_meta_map.begin(); it != vb_meta_map.end(); ++it) {
			const VoxelMetadata &meta = it->value;
			const VoxelMetadata *rmeta = rvb_meta_map.find(it->key);

			ZYLANN_TEST_ASSERT(rmeta != nullptr);
			ZYLANN_TEST_ASSERT(rmeta->get_type() == meta.get_type());

			switch (meta.get_type()) {
				case VoxelMetadata::TYPE_EMPTY:
					break;
				case VoxelMetadata::TYPE_U64:
					ZYLANN_TEST_ASSERT(meta.get_u64() == rmeta->get_u64());
					break;
				case CustomMetadataTest::ID: {
					const CustomMetadataTest &custom = static_cast<const CustomMetadataTest &>(meta.get_custom());
					const CustomMetadataTest &rcustom = static_cast<const CustomMetadataTest &>(rmeta->get_custom());
					ZYLANN_TEST_ASSERT(custom == rcustom);
				} break;
				default:
					ZYLANN_TEST_ASSERT(false);
					break;
			}
		}
	}
}

void test_voxel_buffer_metadata_gd() {
	// Basic get and set (Godot)
	{
		Ref<gd::VoxelBuffer> vb;
		vb.instantiate();
		vb->create(10, 10, 10);

		Array meta;
		meta.push_back("Hello");
		meta.push_back("World");
		meta.push_back(42);

		vb->set_voxel_metadata(Vector3i(1, 2, 3), meta);

		Array read_meta = vb->get_voxel_metadata(Vector3i(1, 2, 3));
		ZYLANN_TEST_ASSERT(read_meta.size() == meta.size());
		ZYLANN_TEST_ASSERT(read_meta == meta);
	}
	// Serialization (Godot)
	{
		Ref<gd::VoxelBuffer> vb;
		vb.instantiate();
		vb->create(10, 10, 10);

		{
			Array meta0;
			meta0.push_back("Hello");
			meta0.push_back("World");
			meta0.push_back(42);
			vb->set_voxel_metadata(Vector3i(1, 2, 3), meta0);
		}
		{
			Dictionary meta1;
			meta1["One"] = 1;
			meta1["Two"] = 2.5;
			meta1["Three"] = Basis();
			vb->set_voxel_metadata(Vector3i(4, 5, 6), meta1);
		}

		BlockSerializer::SerializeResult sresult = BlockSerializer::serialize(vb->get_buffer());
		ZYLANN_TEST_ASSERT(sresult.success);
		std::vector<uint8_t> bytes = sresult.data;

		Ref<gd::VoxelBuffer> vb2;
		vb2.instantiate();

		ZYLANN_TEST_ASSERT(BlockSerializer::deserialize(to_span(bytes), vb2->get_buffer()));

		ZYLANN_TEST_ASSERT(vb2->get_buffer().equals(vb->get_buffer()));

		// `equals` does not compare metadata at the moment, mainly because it's not trivial and there is no use case
		// for it apart from this test, so do it manually

		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb_meta_map = vb->get_buffer().get_voxel_metadata();
		const FlatMapMoveOnly<Vector3i, VoxelMetadata> &vb2_meta_map = vb2->get_buffer().get_voxel_metadata();

		ZYLANN_TEST_ASSERT(vb_meta_map.size() == vb2_meta_map.size());

		for (auto it = vb_meta_map.begin(); it != vb_meta_map.end(); ++it) {
			const VoxelMetadata &meta = it->value;
			ZYLANN_TEST_ASSERT(meta.get_type() == gd::METADATA_TYPE_VARIANT);

			const VoxelMetadata *meta2 = vb2_meta_map.find(it->key);
			ZYLANN_TEST_ASSERT(meta2 != nullptr);
			ZYLANN_TEST_ASSERT(meta2->get_type() == meta.get_type());

			const gd::VoxelMetadataVariant &metav = static_cast<const gd::VoxelMetadataVariant &>(meta.get_custom());
			const gd::VoxelMetadataVariant &meta2v = static_cast<const gd::VoxelMetadataVariant &>(meta2->get_custom());
			ZYLANN_TEST_ASSERT(metav.data == meta2v.data);
		}
	}
}

void test_voxel_mesher_cubes() {
	VoxelBufferInternal vb;
	vb.create(8, 8, 8);
	vb.set_channel_depth(VoxelBufferInternal::CHANNEL_COLOR, VoxelBufferInternal::DEPTH_16_BIT);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(3, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 255, 0, 255).to_u16(), Vector3i(4, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);
	vb.set_voxel(Color8(0, 0, 255, 128).to_u16(), Vector3i(5, 4, 4), VoxelBufferInternal::CHANNEL_COLOR);

	Ref<VoxelMesherCubes> mesher;
	mesher.instantiate();
	mesher->set_color_mode(VoxelMesherCubes::COLOR_RAW);

	VoxelMesher::Input input{ vb, nullptr, nullptr, Vector3i(), 0, false };
	VoxelMesher::Output output;
	mesher->build(output, input);

	const unsigned int opaque_surface_index = VoxelMesherCubes::MATERIAL_OPAQUE;
	const unsigned int transparent_surface_index = VoxelMesherCubes::MATERIAL_TRANSPARENT;

	ZYLANN_TEST_ASSERT(output.surfaces.size() == 2);
	ZYLANN_TEST_ASSERT(output.surfaces[0].arrays.size() > 0);
	ZYLANN_TEST_ASSERT(output.surfaces[1].arrays.size() > 0);

	const PackedVector3Array surface0_vertices = output.surfaces[opaque_surface_index].arrays[Mesh::ARRAY_VERTEX];
	const unsigned int surface0_vertices_count = surface0_vertices.size();

	const PackedVector3Array surface1_vertices = output.surfaces[transparent_surface_index].arrays[Mesh::ARRAY_VERTEX];
	const unsigned int surface1_vertices_count = surface1_vertices.size();

	// println("Surface0:");
	// for (int i = 0; i < surface0_vertices.size(); ++i) {
	// 	println(format("v[{}]: {}", i, surface0_vertices[i]));
	// }
	// println("Surface1:");
	// for (int i = 0; i < surface1_vertices.size(); ++i) {
	// 	println(format("v[{}]: {}", i, surface1_vertices[i]));
	// }

	// Greedy meshing with two cubes of the same color next to each other means it will be a single box.
	// Each side has different normals, so vertices have to be repeated. 6 sides * 4 vertices = 24.
	ZYLANN_TEST_ASSERT(surface0_vertices_count == 24);
	// The transparent cube has less vertices because one of its faces overlaps with a neighbor solid face,
	// so it is culled
	ZYLANN_TEST_ASSERT(surface1_vertices_count == 20);
}

void test_threaded_task_runner() {
	static const uint32_t task_duration_usec = 100'000;

	struct TaskCounter {
		std::atomic_int max_count;
		std::atomic_int current_count;
		std::atomic_int completed_count;

		void reset() {
			max_count = 0;
			current_count = 0;
			completed_count = 0;
		}
	};

	class TestTask : public IThreadedTask {
	public:
		std::shared_ptr<TaskCounter> counter;
		bool completed = false;

		TestTask(std::shared_ptr<TaskCounter> p_counter) : counter(p_counter) {}

		void run(ThreadedTaskContext ctx) override {
			ZN_PROFILE_SCOPE();
			ZN_ASSERT(counter != nullptr);

			++counter->current_count;

			// Update maximum count
			// https://stackoverflow.com/questions/16190078/how-to-atomically-update-a-maximum-value
			int current_count = counter->current_count;
			int prev_max = counter->max_count;
			while (prev_max < current_count && !counter->max_count.compare_exchange_weak(prev_max, current_count)) {
				current_count = counter->current_count;
			}

			Thread::sleep_usec(task_duration_usec);

			--counter->current_count;
			++counter->completed_count;
			completed = true;
		}

		void apply_result() override {
			ZYLANN_TEST_ASSERT(completed);
		}
	};

	struct L {
		static void dequeue_tasks(ThreadedTaskRunner &runner) {
			runner.dequeue_completed_tasks([](IThreadedTask *task) {
				ZN_ASSERT(task != nullptr);
				task->apply_result();
				ZN_DELETE(task);
			});
		}
	};

	const unsigned int test_thread_count = 4;
	const unsigned int hw_concurrency = Thread::get_hardware_concurrency();
	if (hw_concurrency < test_thread_count) {
		ZN_PRINT_WARNING(format(
				"Hardware concurrency is {}, smaller than test requirement {}", test_thread_count, hw_concurrency));
	}

	std::shared_ptr<TaskCounter> parallel_counter = make_unique_instance<TaskCounter>();
	std::shared_ptr<TaskCounter> serial_counter = make_unique_instance<TaskCounter>();

	ThreadedTaskRunner runner;
	runner.set_thread_count(test_thread_count);
	runner.set_batch_count(1);
	runner.set_name("Test");

	// Parallel tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		runner.enqueue(ZN_NEW(TestTask(parallel_counter)), false);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZYLANN_TEST_ASSERT(parallel_counter->completed_count == 16);
	ZYLANN_TEST_ASSERT(parallel_counter->max_count <= test_thread_count);
	ZYLANN_TEST_ASSERT(parallel_counter->current_count == 0);

	// Serial tasks only

	for (unsigned int i = 0; i < 16; ++i) {
		runner.enqueue(ZN_NEW(TestTask(serial_counter)), true);
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZYLANN_TEST_ASSERT(serial_counter->completed_count == 16);
	ZYLANN_TEST_ASSERT(serial_counter->max_count == 1);
	ZYLANN_TEST_ASSERT(serial_counter->current_count == 0);

	// Interleaved

	parallel_counter->reset();
	serial_counter->reset();

	for (unsigned int i = 0; i < 32; ++i) {
		if ((i & 1) == 0) {
			runner.enqueue(ZN_NEW(TestTask(parallel_counter)), false);
		} else {
			runner.enqueue(ZN_NEW(TestTask(serial_counter)), true);
		}
	}

	runner.wait_for_all_tasks();
	L::dequeue_tasks(runner);
	ZYLANN_TEST_ASSERT(parallel_counter->completed_count == 16);
	ZYLANN_TEST_ASSERT(parallel_counter->max_count <= test_thread_count);
	ZYLANN_TEST_ASSERT(parallel_counter->current_count == 0);
	ZYLANN_TEST_ASSERT(serial_counter->completed_count == 16);
	ZYLANN_TEST_ASSERT(serial_counter->max_count == 1);
	ZYLANN_TEST_ASSERT(serial_counter->current_count == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define VOXEL_TEST(fname)                                                                                              \
	print_line(String("Running {0}").format(varray(#fname)));                                                          \
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
	VOXEL_TEST(test_voxel_graph_generator_expressions);
	VOXEL_TEST(test_voxel_graph_generator_texturing);
	VOXEL_TEST(test_island_finder);
	VOXEL_TEST(test_unordered_remove_if);
	VOXEL_TEST(test_instance_data_serialization);
	VOXEL_TEST(test_transform_3d_array_zxy);
	VOXEL_TEST(test_octree_update);
	VOXEL_TEST(test_octree_find_in_box);
	VOXEL_TEST(test_get_curve_monotonic_sections);
	VOXEL_TEST(test_voxel_buffer_create);
	VOXEL_TEST(test_block_serializer);
	VOXEL_TEST(test_block_serializer_stream_peer);
	VOXEL_TEST(test_region_file);
	VOXEL_TEST(test_voxel_stream_region_files);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	VOXEL_TEST(test_fast_noise_2);
#endif
	VOXEL_TEST(test_run_blocky_random_tick);
	VOXEL_TEST(test_flat_map);
	VOXEL_TEST(test_expression_parser);
	VOXEL_TEST(test_voxel_buffer_metadata);
	VOXEL_TEST(test_voxel_buffer_metadata_gd);
	VOXEL_TEST(test_voxel_mesher_cubes);
	VOXEL_TEST(test_threaded_task_runner);

	print_line("------------ Voxel tests end -------------");
}

} // namespace zylann::voxel::tests
