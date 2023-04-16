#include "test_voxel_graph.h"
#include "../generators/graph/node_type_db.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "../storage/voxel_buffer_internal.h"
#include "../util/container_funcs.h"
#include "../util/math/conv.h"
#include "../util/math/sdf.h"
#include "../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../util/string_funcs.h"
#include "test_util.h"
#include "testing.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../util/noise/fast_noise_2.h"
#endif

#include <core/io/resource_loader.h>
#include <modules/noise/fastnoise_lite.h>

namespace zylann::voxel::tests {

using namespace pg;

math::Interval get_sdf_range(const VoxelBufferInternal &block) {
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	math::Interval range = math::Interval::from_single_value(block.get_voxel_f(Vector3i(), channel));
	Vector3i pos;
	const Vector3i size = block.get_size();

	for (pos.z = 0; pos.z < size.z; ++pos.z) {
		for (pos.x = 0; pos.x < size.x; ++pos.x) {
			for (pos.y = 0; pos.y < size.y; ++pos.y) {
				range.add_point(block.get_voxel_f(pos, channel));
			}
		}
	}

	return range;
}

bool check_graph_results_are_equal(VoxelGeneratorGraph &generator1, VoxelGeneratorGraph &generator2, Vector3i origin) {
	{
		const float sd1 = generator1.generate_single(origin, VoxelBufferInternal::CHANNEL_SDF).f;
		const float sd2 = generator2.generate_single(origin, VoxelBufferInternal::CHANNEL_SDF).f;

		if (!Math::is_equal_approx(sd1, sd2)) {
			ZN_PRINT_ERROR(format("sd1: ", sd1));
			ZN_PRINT_ERROR(format("sd2: ", sd1));
			return false;
		}
	}

	const Vector3i block_size(16, 16, 16);

	VoxelBufferInternal block1;
	block1.create(block_size);

	VoxelBufferInternal block2;
	block2.create(block_size);

	// Note, not every graph configuration can be considered invalid when inequal.
	// SDF clipping does create differences that are supposed to be irrelevant for our use cases.
	// So it is important that we test generators with the same SDF clipping options.
	ZN_ASSERT(generator1.get_sdf_clip_threshold() == generator2.get_sdf_clip_threshold());

	generator1.generate_block(VoxelGenerator::VoxelQueryData{ block1, origin, 0 });
	generator2.generate_block(VoxelGenerator::VoxelQueryData{ block2, origin, 0 });

	if (block1.equals(block2)) {
		return true;
	}

	const math::Interval range1 = get_sdf_range(block1);
	const math::Interval range2 = get_sdf_range(block2);
	ZN_PRINT_ERROR(format("When testing box ", Box3i(origin, block_size)));
	ZN_PRINT_ERROR(format("Block1 range: ", range1));
	ZN_PRINT_ERROR(format("Block2 range: ", range2));
	return false;
}

bool check_graph_results_are_equal(VoxelGeneratorGraph &generator1, VoxelGeneratorGraph &generator2) {
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i()));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(-8, -8, -8)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(0, 100, 0)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(0, -100, 0)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(100, 0, 0)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(-100, 0, 0)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(100, 100, 100)));
	ZN_TEST_ASSERT(check_graph_results_are_equal(generator1, generator2, Vector3i(-100, -100, -100)));
	return true;
}

void test_voxel_graph_generator_default_graph_compilation() {
	Ref<VoxelGeneratorGraph> generator_debug;
	Ref<VoxelGeneratorGraph> generator;
	{
		generator_debug.instantiate();
		generator_debug->load_plane_preset();
		pg::CompilationResult result = generator_debug->compile(true);
		ZN_TEST_ASSERT_MSG(result.success,
				String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
	}
	{
		generator.instantiate();
		generator->load_plane_preset();
		pg::CompilationResult result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(result.success,
				String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
	}
	if (generator_debug.is_valid() && generator.is_valid()) {
		ZN_TEST_ASSERT(check_graph_results_are_equal(**generator_debug, **generator));
	}
}

void test_voxel_graph_invalid_connection() {
	Ref<VoxelGraphFunction> graph;
	graph.instantiate();

	VoxelGraphFunction &g = **graph;

	const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
	const uint32_t n_add1 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
	const uint32_t n_add2 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
	const uint32_t n_out = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
	g.add_connection(n_x, 0, n_add1, 0);
	g.add_connection(n_add1, 0, n_add2, 0);
	g.add_connection(n_add2, 0, n_out, 0);

	ZN_TEST_ASSERT(g.can_connect(n_add1, 0, n_add2, 1) == true);
	ZN_TEST_ASSERT_MSG(g.can_connect(n_add1, 0, n_add2, 0) == false, "Adding twice the same connection is not allowed");
	ZN_TEST_ASSERT_MSG(g.can_connect(n_x, 0, n_add2, 0) == false,
			"Adding a connection to a port already connected is not allowed");
	ZN_TEST_ASSERT_MSG(g.can_connect(n_add1, 0, n_add1, 1) == false, "Connecting a node to itself is not allowed");
	ZN_TEST_ASSERT_MSG(g.can_connect(n_add2, 0, n_add1, 1) == false, "Creating a cycle is not allowed");
}

void load_graph_with_sphere_on_plane(VoxelGraphFunction &g, float radius) {
	//      X
	//       \
	//  Z --- Sphere --- Union --- OutSDF
	//       /          /
	//      Y --- Plane
	//

	const uint32_t n_in_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(0, 0));
	const uint32_t n_in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(0, 0));
	const uint32_t n_in_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(0, 0));
	const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(0, 0));
	const uint32_t n_plane = g.create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2());
	const uint32_t n_sphere = g.create_node(VoxelGraphFunction::NODE_SDF_SPHERE, Vector2());
	const uint32_t n_union = g.create_node(VoxelGraphFunction::NODE_SDF_SMOOTH_UNION, Vector2());

	uint32_t union_smoothness_id;
	ZN_ASSERT(NodeTypeDB::get_singleton().try_get_param_index_from_name(
			VoxelGraphFunction::NODE_SDF_SMOOTH_UNION, "smoothness", union_smoothness_id));

	g.add_connection(n_in_x, 0, n_sphere, 0);
	g.add_connection(n_in_y, 0, n_sphere, 1);
	g.add_connection(n_in_z, 0, n_sphere, 2);
	g.set_node_param(n_sphere, 0, radius);
	g.add_connection(n_in_y, 0, n_plane, 0);
	g.set_node_default_input(n_plane, 1, 0.f);
	g.add_connection(n_sphere, 0, n_union, 0);
	g.add_connection(n_plane, 0, n_union, 1);
	g.set_node_param(n_union, union_smoothness_id, 0.f);
	g.add_connection(n_union, 0, n_out_sdf, 0);
}

void load_graph_with_expression(VoxelGraphFunction &g) {
	const uint32_t in_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(0, 0));
	const uint32_t in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(0, 0));
	const uint32_t in_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(0, 0));
	const uint32_t out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(0, 0));
	const uint32_t n_expression = g.create_node(VoxelGraphFunction::NODE_EXPRESSION, Vector2());

	//             0.5
	//                \
	//     0.1   y --- min
	//        \           \
	//   x --- * --- + --- + --- sdf
	//              /
	//     0.2 --- *
	//            /
	//           z

	g.set_node_param(n_expression, 0, "0.1 * x + 0.2 * z + min(y, 0.5)");
	PackedStringArray var_names;
	var_names.push_back("x");
	var_names.push_back("y");
	var_names.push_back("z");
	g.set_expression_node_inputs(n_expression, var_names);

	g.add_connection(in_x, 0, n_expression, 0);
	g.add_connection(in_y, 0, n_expression, 1);
	g.add_connection(in_z, 0, n_expression, 2);
	g.add_connection(n_expression, 0, out_sdf, 0);
}

void load_graph_with_expression_and_noises(VoxelGraphFunction &g, Ref<ZN_FastNoiseLite> *out_zfnl) {
	//                       SdfPreview
	//                      /
	//     X --- FastNoise2D
	//      \/              \
	//      /\               \
	//     Z --- Noise2D ----- a+b+c --- OutputSDF
	//                        /
	//     Y --- SdfPlane ----

	const uint32_t in_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(0, 0));
	const uint32_t in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(0, 0));
	const uint32_t in_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(0, 0));
	const uint32_t out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(0, 0));
	const uint32_t n_fn2d = g.create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2());
	const uint32_t n_n2d = g.create_node(VoxelGraphFunction::NODE_NOISE_2D, Vector2());
	const uint32_t n_plane = g.create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2());
	const uint32_t n_expr = g.create_node(VoxelGraphFunction::NODE_EXPRESSION, Vector2());
	const uint32_t n_preview = g.create_node(VoxelGraphFunction::NODE_SDF_PREVIEW, Vector2());

	g.set_node_param(n_expr, 0, "a+b+c");
	PackedStringArray var_names;
	var_names.push_back("a");
	var_names.push_back("b");
	var_names.push_back("c");
	g.set_expression_node_inputs(n_expr, var_names);

	Ref<ZN_FastNoiseLite> zfnl;
	zfnl.instantiate();
	g.set_node_param(n_fn2d, 0, zfnl);

	Ref<FastNoiseLite> fnl;
	fnl.instantiate();
	g.set_node_param(n_n2d, 0, fnl);

	g.add_connection(in_x, 0, n_fn2d, 0);
	g.add_connection(in_x, 0, n_n2d, 0);
	g.add_connection(in_z, 0, n_fn2d, 1);
	g.add_connection(in_z, 0, n_n2d, 1);
	g.add_connection(in_y, 0, n_plane, 0);
	g.add_connection(n_fn2d, 0, n_expr, 0);
	g.add_connection(n_fn2d, 0, n_preview, 0);
	g.add_connection(n_n2d, 0, n_expr, 1);
	g.add_connection(n_plane, 0, n_expr, 2);
	g.add_connection(n_expr, 0, out_sdf, 0);

	if (out_zfnl != nullptr) {
		*out_zfnl = zfnl;
	}
}

void load_graph_with_clamp(VoxelGraphFunction &g, float ramp_half_size) {
	// Two planes of different height, with a 45-degrees ramp along the X axis between them.
	// The plane is higher in negative X, and lower in positive X.
	//
	//   X --- Clamp --- + --- Out
	//                  /
	//                 Y

	const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
	const uint32_t n_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2());
	// Not using CLAMP_C for testing simplification
	const uint32_t n_clamp = g.create_node(VoxelGraphFunction::NODE_CLAMP, Vector2());
	const uint32_t n_add = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
	const uint32_t n_out = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());

	g.set_node_default_input(n_clamp, 1, -ramp_half_size);
	g.set_node_default_input(n_clamp, 2, ramp_half_size);

	g.add_connection(n_x, 0, n_clamp, 0);
	g.add_connection(n_clamp, 0, n_add, 0);
	g.add_connection(n_y, 0, n_add, 1);
	g.add_connection(n_add, 0, n_out, 0);
}

void test_voxel_graph_clamp_simplification() {
	// The CLAMP node is replaced with a CLAMP_C node on compilation.
	// This tests that the generator still behaves properly.
	static const float RAMP_HALF_SIZE = 4.f;
	struct L {
		static Ref<VoxelGeneratorGraph> create_graph(bool debug) {
			Ref<VoxelGeneratorGraph> generator;
			generator.instantiate();
			ZN_ASSERT(generator->get_main_function().is_valid());
			load_graph_with_clamp(**generator->get_main_function(), RAMP_HALF_SIZE);
			pg::CompilationResult result = generator->compile(debug);
			ZN_TEST_ASSERT_MSG(result.success,
					String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
			return generator;
		}
		static void test_locations(VoxelGeneratorGraph &g) {
			const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
			const float sd_on_higher_side_below_ground =
					g.generate_single(Vector3i(-RAMP_HALF_SIZE - 10, 0, 0), channel).f;
			const float sd_on_higher_side_above_ground =
					g.generate_single(Vector3i(-RAMP_HALF_SIZE - 10, RAMP_HALF_SIZE + 2, 0), channel).f;
			const float sd_on_lower_side_above_ground =
					g.generate_single(Vector3i(RAMP_HALF_SIZE + 10, 0, 0), channel).f;
			const float sd_on_lower_side_below_ground =
					g.generate_single(Vector3i(RAMP_HALF_SIZE + 10, -RAMP_HALF_SIZE - 2, 0), channel).f;

			ZN_TEST_ASSERT(sd_on_lower_side_above_ground > 0.f);
			ZN_TEST_ASSERT(sd_on_lower_side_below_ground < 0.f);
			ZN_TEST_ASSERT(sd_on_higher_side_above_ground > 0.f);
			ZN_TEST_ASSERT(sd_on_higher_side_below_ground < 0.f);
		}
	};
	Ref<VoxelGeneratorGraph> generator_debug = L::create_graph(true);
	Ref<VoxelGeneratorGraph> generator = L::create_graph(false);
	ZN_TEST_ASSERT(check_graph_results_are_equal(**generator_debug, **generator));
	L::test_locations(**generator);
	L::test_locations(**generator_debug);
}

void test_voxel_graph_generator_expressions() {
	struct L {
		static Ref<VoxelGeneratorGraph> create_graph(bool debug) {
			Ref<VoxelGeneratorGraph> generator;
			generator.instantiate();
			ZN_ASSERT(generator->get_main_function().is_valid());
			load_graph_with_expression(**generator->get_main_function());
			pg::CompilationResult result = generator->compile(debug);
			ZN_TEST_ASSERT_MSG(result.success,
					String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
			return generator;
		}
	};
	Ref<VoxelGeneratorGraph> generator_debug = L::create_graph(true);
	Ref<VoxelGeneratorGraph> generator = L::create_graph(false);
	ZN_TEST_ASSERT(check_graph_results_are_equal(**generator_debug, **generator));
}

void test_voxel_graph_generator_expressions_2() {
	Ref<ZN_FastNoiseLite> zfnl;
	{
		Ref<VoxelGeneratorGraph> generator_debug;
		{
			generator_debug.instantiate();
			Ref<VoxelGraphFunction> graph = generator_debug->get_main_function();
			ZN_ASSERT(graph.is_valid());
			load_graph_with_expression_and_noises(**graph, &zfnl);
			pg::CompilationResult result = generator_debug->compile(true);
			ZN_TEST_ASSERT_MSG(result.success,
					String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));

			generator_debug->generate_single(Vector3i(1, 2, 3), VoxelBufferInternal::CHANNEL_SDF);

			std::vector<VoxelGeneratorGraph::NodeProfilingInfo> profiling_info;
			generator_debug->debug_measure_microseconds_per_voxel(false, &profiling_info);
			ZN_TEST_ASSERT(profiling_info.size() >= 4);
			for (const VoxelGeneratorGraph::NodeProfilingInfo &info : profiling_info) {
				ZN_TEST_ASSERT(graph->has_node(info.node_id));
			}
		}

		Ref<VoxelGeneratorGraph> generator;
		{
			generator.instantiate();
			ZN_ASSERT(generator->get_main_function().is_valid());
			load_graph_with_expression_and_noises(**generator->get_main_function(), nullptr);
			pg::CompilationResult result = generator->compile(false);
			ZN_TEST_ASSERT_MSG(result.success,
					String("Failed to compile graph: {0}: {1}").format(varray(result.node_id, result.message)));
		}

		ZN_TEST_ASSERT(check_graph_results_are_equal(**generator_debug, **generator));
	}

	// Making sure it didn't leak
	ZN_TEST_ASSERT(zfnl.is_valid());
	ZN_TEST_ASSERT(zfnl->get_reference_count() == 1);
}

void test_voxel_graph_generator_texturing() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();

	VoxelGraphFunction &g = **generator->get_main_function();

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

	const uint32_t in_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2(0, 0));
	const uint32_t in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2(0, 0));
	/*const uint32_t in_z =*/g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2(0, 0));
	const uint32_t out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(0, 0));
	const uint32_t n_clamp = g.create_node(VoxelGraphFunction::NODE_CLAMP_C, Vector2(0, 0));
	const uint32_t n_sub0 = g.create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2(0, 0));
	const uint32_t n_sub1 = g.create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2(0, 0));
	const uint32_t out_weight0 = g.create_node(VoxelGraphFunction::NODE_OUTPUT_WEIGHT, Vector2(0, 0));
	const uint32_t out_weight1 = g.create_node(VoxelGraphFunction::NODE_OUTPUT_WEIGHT, Vector2(0, 0));

	g.set_node_default_input(n_sub1, 0, 1.0);
	g.set_node_param(n_clamp, 0, 0.0);
	g.set_node_param(n_clamp, 1, 1.0);
	g.set_node_param(out_weight0, 0, 0);
	g.set_node_param(out_weight1, 0, 1);

	g.add_connection(in_y, 0, n_sub0, 0);
	g.add_connection(in_x, 0, n_sub0, 1);
	g.add_connection(n_sub0, 0, out_sdf, 0);
	g.add_connection(in_y, 0, n_clamp, 0);
	g.add_connection(n_clamp, 0, n_sub1, 1);
	g.add_connection(n_sub1, 0, out_weight0, 0);
	g.add_connection(n_clamp, 0, out_weight1, 0);

	pg::CompilationResult compilation_result = generator->compile(false);
	ZN_TEST_ASSERT_MSG(compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));

	// Single value tests
	{
		const float sdf_must_be_in_air =
				generator->generate_single(Vector3i(-2, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
		const float sdf_must_be_in_ground =
				generator->generate_single(Vector3i(2, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
		ZN_TEST_ASSERT(sdf_must_be_in_air > 0.f);
		ZN_TEST_ASSERT(sdf_must_be_in_ground < 0.f);

		uint32_t out_weight0_buffer_index;
		uint32_t out_weight1_buffer_index;
		ZN_TEST_ASSERT(generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight0, 0 }, out_weight0_buffer_index));
		ZN_TEST_ASSERT(generator->try_get_output_port_address(
				ProgramGraph::PortLocation{ out_weight1, 0 }, out_weight1_buffer_index));

		// Sample two points 1 unit below ground at to heights on the slope

		{
			const float sdf = generator->generate_single(Vector3i(-2, -3, 0), VoxelBufferInternal::CHANNEL_SDF).f;
			ZN_TEST_ASSERT(sdf < 0.f);
			const pg::Runtime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const pg::Runtime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const pg::Runtime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ZN_TEST_ASSERT(out_weight0_buffer.size >= 1);
			ZN_TEST_ASSERT(out_weight0_buffer.data != nullptr);
			ZN_TEST_ASSERT(out_weight0_buffer.data[0] >= 1.f);

			ZN_TEST_ASSERT(out_weight1_buffer.size >= 1);
			ZN_TEST_ASSERT(out_weight1_buffer.data != nullptr);
			ZN_TEST_ASSERT(out_weight1_buffer.data[0] <= 0.f);
		}
		{
			const float sdf = generator->generate_single(Vector3i(2, 1, 0), VoxelBufferInternal::CHANNEL_SDF).f;
			ZN_TEST_ASSERT(sdf < 0.f);
			const pg::Runtime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

			const pg::Runtime::Buffer &out_weight0_buffer = state.get_buffer(out_weight0_buffer_index);
			const pg::Runtime::Buffer &out_weight1_buffer = state.get_buffer(out_weight1_buffer_index);

			ZN_TEST_ASSERT(out_weight0_buffer.size >= 1);
			ZN_TEST_ASSERT(out_weight0_buffer.data != nullptr);
			ZN_TEST_ASSERT(out_weight0_buffer.data[0] <= 0.f);

			ZN_TEST_ASSERT(out_weight1_buffer.size >= 1);
			ZN_TEST_ASSERT(out_weight1_buffer.data != nullptr);
			ZN_TEST_ASSERT(out_weight1_buffer.data[0] >= 1.f);
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
								ZN_TEST_ASSERT(weights[i] >= WEIGHT_MAX);
							} else {
								ZN_TEST_ASSERT(weights[i] <= 0);
							}
							break;
						case 1:
							if (weight1_must_be_1) {
								ZN_TEST_ASSERT(weights[i] >= WEIGHT_MAX);
							} else {
								ZN_TEST_ASSERT(weights[i] <= 0);
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
		// const pg::Runtime::State &state = VoxelGeneratorGraph::get_last_state_from_current_thread();

		// Try first without optimization
		generator->set_use_optimized_execution_map(false);
		L::do_block_tests(generator);
		// Try with optimization
		generator->set_use_optimized_execution_map(true);
		L::do_block_tests(generator);
	}
}

void test_voxel_graph_equivalence_merging() {
	{
		// Basic graph with two equivalent branches

		//        1
		//         \
		//    X --- +                         1
		//           \             =>          \
		//        1   + --- Out           X --- + === + --- Out
		//         \ /
		//    X --- +

		Ref<VoxelGeneratorGraph> graph;
		graph.instantiate();
		VoxelGraphFunction &g = **graph->get_main_function();
		const uint32_t n_x1 = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_add1 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_x2 = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_add2 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_add3 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_out = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.set_node_default_input(n_add1, 0, 1.0);
		g.set_node_default_input(n_add2, 0, 1.0);
		g.add_connection(n_x1, 0, n_add1, 1);
		g.add_connection(n_add1, 0, n_add3, 0);
		g.add_connection(n_x2, 0, n_add2, 1);
		g.add_connection(n_add2, 0, n_add3, 1);
		g.add_connection(n_add3, 0, n_out, 0);
		pg::CompilationResult result = graph->compile(false);
		ZN_TEST_ASSERT(result.success);
		ZN_TEST_ASSERT(result.expanded_nodes_count == 4);
		const VoxelSingleValue value = graph->generate_single(Vector3i(10, 0, 0), VoxelBufferInternal::CHANNEL_SDF);
		ZN_TEST_ASSERT(value.f == 22);
	}
	{
		// Same as previous but the X input node is shared

		//          1
		//           \
		//    X ----- +
		//     \       \
		//      \   1   + --- Out
		//       \   \ /
		//        --- +

		Ref<VoxelGeneratorGraph> graph;
		graph.instantiate();
		VoxelGraphFunction &g = **graph->get_main_function();
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_add1 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_add2 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_add3 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_out = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.set_node_default_input(n_add1, 0, 1.0);
		g.set_node_default_input(n_add2, 0, 1.0);
		g.add_connection(n_x, 0, n_add1, 1);
		g.add_connection(n_add1, 0, n_add3, 0);
		g.add_connection(n_x, 0, n_add2, 1);
		g.add_connection(n_add2, 0, n_add3, 1);
		g.add_connection(n_add3, 0, n_out, 0);
		pg::CompilationResult result = graph->compile(false);
		ZN_TEST_ASSERT(result.success);
		ZN_TEST_ASSERT(result.expanded_nodes_count == 4);
		const VoxelSingleValue value = graph->generate_single(Vector3i(10, 0, 0), VoxelBufferInternal::CHANNEL_SDF);
		ZN_TEST_ASSERT(value.f == 22);
	}
}

void print_sdf_as_ascii(const VoxelBufferInternal &vb) {
	Vector3i pos;
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	for (pos.y = 0; pos.y < vb.get_size().y; ++pos.y) {
		println(format("Y = {}", pos.y));
		for (pos.z = 0; pos.z < vb.get_size().z; ++pos.z) {
			std::string s;
			std::string s2;
			for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
				const float sd = vb.get_voxel_f(pos, channel);
				char c;
				if (sd < -0.9f) {
					c = '=';
				} else if (sd < 0.0f) {
					c = '-';
				} else if (sd == 0.f) {
					c = ' ';
				} else if (sd < 0.9f) {
					c = '+';
				} else {
					c = '#';
				}
				s += c;
				s += " ";
				std::string n = std::to_string(math::clamp(int(sd * 1000.f), -999, 999));
				while (n.size() < 4) {
					n = " " + n;
				}
				s2 += n;
				s2 += " ";
			}
			s += " | ";
			s += s2;
			println(s);
		}
	}
}

/*bool find_different_voxel(const VoxelBufferInternal &vb1, const VoxelBufferInternal &vb2, Vector3i *out_pos,
		unsigned int *out_channel_index) {
	ZN_ASSERT(vb1.get_size() == vb2.get_size());
	Vector3i pos;
	for (pos.y = 0; pos.y < vb1.get_size().y; ++pos.y) {
		for (pos.z = 0; pos.z < vb1.get_size().z; ++pos.z) {
			for (pos.x = 0; pos.x < vb1.get_size().x; ++pos.x) {
				for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS;
						++channel_index) {
					const uint64_t v1 = vb1.get_voxel(pos, channel_index);
					const uint64_t v2 = vb2.get_voxel(pos, channel_index);
					if (v1 != v2) {
						if (out_pos != nullptr) {
							*out_pos = pos;
						}
						if (out_channel_index != nullptr) {
							*out_channel_index = channel_index;
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}*/

void test_voxel_graph_generate_block_with_input_sdf() {
	static const int BLOCK_SIZE = 16;
	static const float SPHERE_RADIUS = 6;

	struct L {
		static void load_graph(VoxelGraphFunction &g) {
			// Just outputting the input
			const uint32_t n_in_sdf = g.create_node(VoxelGraphFunction::NODE_INPUT_SDF, Vector2());
			const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
			g.add_connection(n_in_sdf, 0, n_out_sdf, 0);
		}

		static void test(bool subdivision_enabled, int subdivision_size) {
			// Create generator
			Ref<VoxelGeneratorGraph> generator;
			generator.instantiate();
			L::load_graph(**generator->get_main_function());
			const pg::CompilationResult compilation_result = generator->compile(false);
			ZN_TEST_ASSERT_MSG(compilation_result.success,
					String("Failed to compile graph: {0}: {1}")
							.format(varray(compilation_result.node_id, compilation_result.message)));

			// Create buffer containing part of a sphere
			VoxelBufferInternal buffer;
			buffer.create(Vector3i(BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE));
			const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
			const VoxelBufferInternal::Depth depth = buffer.get_channel_depth(channel);
			const float sd_scale = VoxelBufferInternal::get_sdf_quantization_scale(depth);
			for (int z = 0; z < buffer.get_size().z; ++z) {
				for (int x = 0; x < buffer.get_size().x; ++x) {
					for (int y = 0; y < buffer.get_size().y; ++y) {
						// Sphere at origin
						const float sd = math::sdf_sphere(Vector3(x, y, z), Vector3(), SPHERE_RADIUS);
						buffer.set_voxel_f(sd * sd_scale, Vector3i(x, y, z), channel);
					}
				}
			}

			// Make a backup before running the generator
			VoxelBufferInternal buffer_before;
			buffer_before.create(buffer.get_size());
			buffer_before.copy_from(buffer);

			generator->set_use_subdivision(subdivision_enabled);
			generator->set_subdivision_size(subdivision_size);
			generator->generate_block(VoxelGenerator::VoxelQueryData{ buffer, Vector3i(), 0 });

			/*if (!buffer.equals(buffer_before)) {
				println("Buffer before:");
				print_sdf_as_ascii(buffer_before);
				println("Buffer after:");
				print_sdf_as_ascii(buffer);
				Vector3i different_pos;
				unsigned int different_channel;
				if (find_different_voxel(buffer_before, buffer, &different_pos, &different_channel)) {
					const uint64_t v1 = buffer_before.get_voxel(different_pos, different_channel);
					const uint64_t v2 = buffer.get_voxel(different_pos, different_channel);
					println(format("Different position: {}, v1={}, v2={}", different_pos, v1, v2));
				}
			}*/
			ZN_TEST_ASSERT(sd_equals_approx(buffer, buffer_before));
		}
	};

	L::test(false, BLOCK_SIZE / 2);
	L::test(true, BLOCK_SIZE / 2);
}

Ref<VoxelGraphFunction> create_pass_through_function() {
	Ref<VoxelGraphFunction> func;
	func.instantiate();
	{
		VoxelGraphFunction &g = **func;
		// Pass through
		// X --- OutSDF
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_out_sdf, 0);

		func->auto_pick_inputs_and_outputs();
	}
	return func;
}

void test_voxel_graph_functions_pass_through() {
	Ref<VoxelGraphFunction> func = create_pass_through_function();
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	{
		VoxelGraphFunction &g = **generator->get_main_function();
		// X --- Func --- OutSDF
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_f = g.create_function_node(func, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_f, 0);
		g.add_connection(n_f, 0, n_out_sdf, 0);
	}
	const pg::CompilationResult compilation_result = generator->compile(false);
	ZN_TEST_ASSERT_MSG(compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));
	const float f = generator->generate_single(Vector3i(42, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
	ZN_TEST_ASSERT(f == 42.f);
}

void test_voxel_graph_functions_nested_pass_through() {
	Ref<VoxelGraphFunction> func1 = create_pass_through_function();

	// Minimal function using another
	Ref<VoxelGraphFunction> func2;
	func2.instantiate();
	{
		VoxelGraphFunction &g = **func2;
		// Nested pass through
		// X --- Func1 --- OutSDF
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_f = g.create_function_node(func1, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_f, 0);
		g.add_connection(n_f, 0, n_out_sdf, 0);

		func2->auto_pick_inputs_and_outputs();
	}

	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	{
		VoxelGraphFunction &g = **generator->get_main_function();
		// X --- Func2 --- OutSDF
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_f = g.create_function_node(func2, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_f, 0);
		g.add_connection(n_f, 0, n_out_sdf, 0);
	}
	const pg::CompilationResult compilation_result = generator->compile(false);
	ZN_TEST_ASSERT_MSG(compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));
	const float f = generator->generate_single(Vector3i(42, 0, 0), VoxelBufferInternal::CHANNEL_SDF).f;
	ZN_TEST_ASSERT(f == 42.f);
}

void test_voxel_graph_functions_autoconnect() {
	Ref<VoxelGraphFunction> func;
	func.instantiate();
	const float sphere_radius = 10.f;
	{
		VoxelGraphFunction &g = **func;
		// Sphere --- OutSDF
		const uint32_t n_sphere = g.create_node(VoxelGraphFunction::NODE_SDF_SPHERE, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_sphere, 0, n_out_sdf, 0);
		g.set_node_param(n_sphere, 0, sphere_radius);

		g.auto_pick_inputs_and_outputs();
	}

	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	const float z_offset = 5.f;
	{
		VoxelGraphFunction &g = **generator->get_main_function();
		//      X (auto)
		//              \
		//  Y (auto) --- Func --- OutSDF
		//              /
		//     Z --- Add+5
		//
		const uint32_t n_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2());
		const uint32_t n_add = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
		const uint32_t n_f = g.create_function_node(func, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_z, 0, n_add, 0);
		g.set_node_default_input(n_add, 1, z_offset);
		g.add_connection(n_add, 0, n_f, 2);
		g.add_connection(n_f, 0, n_out_sdf, 0);
	}
	const pg::CompilationResult compilation_result = generator->compile(false);
	ZN_TEST_ASSERT_MSG(compilation_result.success,
			String("Failed to compile graph: {0}: {1}")
					.format(varray(compilation_result.node_id, compilation_result.message)));
	FixedArray<Vector3i, 3> positions;
	positions[0] = Vector3i(1, 1, 1);
	positions[1] = Vector3i(20, 7, -4);
	positions[2] = Vector3i(-5, 0, 18);
	for (const Vector3i &pos : positions) {
		const float sd = generator->generate_single(pos, VoxelBufferInternal::CHANNEL_SDF).f;
		const float expected = math::length(Vector3f(pos.x, pos.y, pos.z + z_offset)) - sphere_radius;
		ZN_TEST_ASSERT(Math::is_equal_approx(sd, expected));
	}
}

void test_voxel_graph_functions_io_mismatch() {
	Ref<VoxelGraphFunction> func;
	func.instantiate();

	// X --- Add --- OutSDF
	//      /
	//     Y
	const uint32_t fn_x = func->create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
	const uint32_t fn_y = func->create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2());
	const uint32_t fn_add = func->create_node(VoxelGraphFunction::NODE_ADD, Vector2());
	const uint32_t fn_out_sdf = func->create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
	func->add_connection(fn_x, 0, fn_add, 0);
	func->add_connection(fn_y, 0, fn_add, 1);
	func->add_connection(fn_add, 0, fn_out_sdf, 0);
	func->auto_pick_inputs_and_outputs();

	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	{
		VoxelGraphFunction &g = **generator->get_main_function();
		// X --- Func --- OutSDF
		//      /
		//     Y
		const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2());
		const uint32_t n_f = g.create_function_node(func, Vector2());
		const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		g.add_connection(n_x, 0, n_f, 0);
		g.add_connection(n_y, 0, n_f, 1);
		g.add_connection(n_f, 0, n_out_sdf, 0);
	}
	{
		const pg::CompilationResult compilation_result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(compilation_result.success,
				String("Failed to compile graph: {0}: {1}")
						.format(varray(compilation_result.node_id, compilation_result.message)));
	}

	// Now remove an input from the function, and see how it goes
	{
		FixedArray<VoxelGraphFunction::Port, 1> inputs;
		inputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_INPUT_X, "x" };
		FixedArray<VoxelGraphFunction::Port, 1> outputs;
		outputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_OUTPUT_SDF, "sdf" };
		func->set_io_definitions(to_span(inputs), to_span(outputs));
	}
	{
		const pg::CompilationResult compilation_result = generator->compile(false);
		// Compiling should fail, but not crash
		ZN_TEST_ASSERT(compilation_result.success == false);
		ZN_PRINT_VERBOSE(format("Compiling failed with message '{}'", compilation_result.message));
	}
	generator->get_main_function()->update_function_nodes(nullptr);
	{
		const pg::CompilationResult compilation_result = generator->compile(false);
		// Compiling should work now
		ZN_TEST_ASSERT(compilation_result.success == true);
	}
}

void test_voxel_graph_functions_misc() {
	static const float func_custom_input_defval = 42.f;

	struct L {
		static Ref<VoxelGraphFunction> create_misc_function() {
			Ref<VoxelGraphFunction> func;
			func.instantiate();
			{
				VoxelGraphFunction &g = **func;
				//
				//          X              OutCustom
				//           \
				//       Z -- Add --- Add --- OutSDF
				//                   /
				//           InCustom
				//
				//   Y(unused)
				//
				const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
				const uint32_t n_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2());
				const uint32_t n_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2());
				const uint32_t n_add1 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
				const uint32_t n_add2 = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2());
				const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
				const uint32_t n_in_custom = g.create_node(VoxelGraphFunction::NODE_CUSTOM_INPUT, Vector2());
				const uint32_t n_out_custom = g.create_node(VoxelGraphFunction::NODE_CUSTOM_OUTPUT, Vector2());

				g.set_node_name(n_in_custom, "custom_input");
				g.set_node_name(n_out_custom, "custom_output");

				g.add_connection(n_x, 0, n_add1, 0);
				g.add_connection(n_z, 0, n_add1, 1);
				g.add_connection(n_add1, 0, n_add2, 0);
				g.add_connection(n_in_custom, 0, n_add2, 1);
				g.add_connection(n_add2, 0, n_out_sdf, 0);
			}
			return func;
		}

		static Ref<VoxelGeneratorGraph> create_generator(Ref<VoxelGraphFunction> func, int input_count) {
			Ref<VoxelGeneratorGraph> generator;
			generator.instantiate();
			//      X
			//       \ 
			//  Z --- Func --- OutSDF
			//
			{
				VoxelGraphFunction &g = **generator->get_main_function();

				const uint32_t n_x = g.create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
				const uint32_t n_z = g.create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2());
				const uint32_t n_f = g.create_function_node(func, Vector2());
				const uint32_t n_out = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());

				if (input_count == 4) {
					g.set_node_default_input(n_f, 3, func_custom_input_defval);
					// This one shouldn't matter, it's unused, but defined still
					g.set_node_default_input(n_f, 2, 12345);
				}

				g.add_connection(n_x, 0, n_f, 0);
				g.add_connection(n_z, 0, n_f, 1);
				g.add_connection(n_f, 0, n_out, 0);
			}

			return generator;
		}
	};

	// Regular test
	{
		Ref<VoxelGraphFunction> func = L::create_misc_function();
		func->auto_pick_inputs_and_outputs();
		ZN_TEST_ASSERT(func->get_input_definitions().size() == 4);
		ZN_TEST_ASSERT(func->get_output_definitions().size() == 2);

		Ref<VoxelGeneratorGraph> generator = L::create_generator(func, 4);

		const pg::CompilationResult compilation_result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(compilation_result.success,
				String("Failed to compile graph: {0}: {1}")
						.format(varray(compilation_result.node_id, compilation_result.message)));

		const Vector3i pos(1, 2, 3);
		const float sd = generator->generate_single(pos, VoxelBufferInternal::CHANNEL_SDF).f;
		const float expected = float(pos.x) + float(pos.z) + func_custom_input_defval;
		ZN_TEST_ASSERT(Math::is_equal_approx(sd, expected));
	}
	// More input nodes than inputs, but should still compile
	{
		Ref<VoxelGraphFunction> func = L::create_misc_function();
		FixedArray<VoxelGraphFunction::Port, 2> inputs;
		inputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_INPUT_X, "x" };
		inputs[1] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_INPUT, "custom_input" };
		// 2 input nodes don't have corresponding inputs
		FixedArray<VoxelGraphFunction::Port, 2> outputs;
		outputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_OUTPUT_SDF, "sdf" };
		outputs[1] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_OUTPUT, "custom_output" };
		func->set_io_definitions(to_span(inputs), to_span(outputs));

		Ref<VoxelGeneratorGraph> generator = L::create_generator(func, 2);

		const pg::CompilationResult compilation_result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(compilation_result.success,
				String("Failed to compile graph: {0}: {1}")
						.format(varray(compilation_result.node_id, compilation_result.message)));
	}
	// Less I/O nodes than I/Os, but should still compile
	{
		Ref<VoxelGraphFunction> func = L::create_misc_function();
		FixedArray<VoxelGraphFunction::Port, 5> inputs;
		inputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_INPUT_X, "x" };
		inputs[1] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_INPUT, "custom_input" };
		inputs[2] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_INPUT, "custom_input2" };
		inputs[3] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_INPUT, "custom_input3" };
		inputs[4] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_INPUT, "custom_input4" };
		// 2 input nodes don't have corresponding inputs
		FixedArray<VoxelGraphFunction::Port, 3> outputs;
		outputs[0] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_OUTPUT_SDF, "sdf" };
		outputs[1] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_OUTPUT, "custom_output" };
		outputs[2] = VoxelGraphFunction::Port{ VoxelGraphFunction::NODE_CUSTOM_OUTPUT, "custom_output2" };
		func->set_io_definitions(to_span(inputs), to_span(outputs));

		Ref<VoxelGeneratorGraph> generator = L::create_generator(func, 2);

		const pg::CompilationResult compilation_result = generator->compile(false);
		ZN_TEST_ASSERT_MSG(compilation_result.success,
				String("Failed to compile graph: {0}: {1}")
						.format(varray(compilation_result.node_id, compilation_result.message)));
	}
}

void test_voxel_graph_issue461() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	generator->get_main_function()->create_node(VoxelGraphFunction::NODE_OUTPUT_TYPE, Vector2(), -69);
	generator->debug_load_waves_preset();
	generator->debug_load_waves_preset();
	// This used to crash
	VoxelGenerator::ShaderSourceData ssd;
	generator->get_shader_source(ssd);
}

template <typename T>
void get_node_types(const NodeTypeDB &type_db, std::vector<VoxelGraphFunction::NodeTypeID> &types, T predicate) {
	for (unsigned int i = 0; i < VoxelGraphFunction::NODE_TYPE_COUNT; ++i) {
		const NodeType &type = type_db.get_type(i);
		if (predicate(type)) {
			types.push_back(VoxelGraphFunction::NodeTypeID(i));
		}
	}
}

// The goal of this test is to find crashes. It will probably cause errors, but should not crash.
void test_voxel_graph_fuzzing() {
	struct L {
		static String make_random_name(RandomPCG &rng) {
			String name;
			const int len = rng.rand() % 8;
			// Note, we let empty names happen.
			for (int i = 0; i < len; ++i) {
				const char c = 'a' + (rng.rand() % ('z' - 'a'));
				name += c;
			}
			return name;
		}

		static void make_random_graph(VoxelGraphFunction &g, RandomPCG &rng, bool allow_custom_io) {
			const int input_count = rng.rand() % 4;
			const int output_count = rng.rand() % 4;
			const int intermediary_node_count = rng.rand() % 8;

			const NodeTypeDB &type_db = NodeTypeDB::get_singleton();

			std::vector<VoxelGraphFunction::NodeTypeID> input_types;
			get_node_types(type_db, input_types,
					[](const NodeType &t) { //
						return t.category == CATEGORY_INPUT;
					});

			std::vector<VoxelGraphFunction::NodeTypeID> output_types;
			get_node_types(type_db, output_types,
					[](const NodeType &t) { //
						return t.category == CATEGORY_OUTPUT;
					});

			if (!allow_custom_io) {
				unordered_remove_value(input_types, VoxelGraphFunction::NODE_CUSTOM_INPUT);
				unordered_remove_value(output_types, VoxelGraphFunction::NODE_CUSTOM_OUTPUT);
			}

			for (int i = 0; i < input_count; ++i) {
				const VoxelGraphFunction::NodeTypeID input_type = input_types[rng.rand() % input_types.size()];
				const uint32_t n = g.create_node(input_type, Vector2());
				g.set_node_name(n, make_random_name(rng));
			}

			for (int i = 0; i < output_count; ++i) {
				const VoxelGraphFunction::NodeTypeID output_type = output_types[rng.rand() % output_types.size()];
				const uint32_t n = g.create_node(output_type, Vector2());
				g.set_node_name(n, make_random_name(rng));
			}

			std::vector<VoxelGraphFunction::NodeTypeID> node_types;
			get_node_types(type_db, node_types,
					[](const NodeType &t) { //
						return t.category != CATEGORY_OUTPUT && t.category != CATEGORY_INPUT;
					});

			for (int i = 0; i < intermediary_node_count; ++i) {
				const VoxelGraphFunction::NodeTypeID type = node_types[rng.rand() % node_types.size()];
				g.create_node(type, Vector2());
			}

			PackedInt32Array node_ids = g.get_node_ids();
			if (node_ids.size() == 0) {
				ZN_PRINT_VERBOSE("Empty graph");
				return;
			}
			const int connection_attempts = rng.rand() % (node_ids.size() + 1);

			for (int i = 0; i < connection_attempts; ++i) {
				const int src_node_id = node_ids[rng.rand() % node_ids.size()];
				const int dst_node_id = node_ids[rng.rand() % node_ids.size()];

				const int src_output_count = g.get_node_output_count(src_node_id);
				const int dst_input_count = g.get_node_input_count(dst_node_id);

				if (src_output_count == 0 || dst_input_count == 0) {
					continue;
				}

				const int src_output_index = rng.rand() % src_output_count;
				const int dst_input_index = rng.rand() % dst_input_count;

				if (g.can_connect(src_node_id, src_output_index, dst_node_id, dst_input_index)) {
					g.add_connection(src_node_id, src_output_index, dst_node_id, dst_input_index);
				}
			}
		}
	};

	const int attempts = 1000;

	RandomPCG rng;
	rng.seed(131183);

	int successful_compiles_count = 0;

	// print_line("--- Begin of zone with possible errors ---");

	for (int i = 0; i < attempts; ++i) {
		ZN_PRINT_VERBOSE(format("Testing random graph #{}", i));
		Ref<VoxelGeneratorGraph> generator;
		generator.instantiate();
		L::make_random_graph(**generator->get_main_function(), rng,
				// Disallowing custom I/Os because VoxelGeneratorGraph cannot handle them at the moment
				false);
		pg::CompilationResult compilation_result = generator->compile(false);
		if (compilation_result.success) {
			generator->generate_single(Vector3i(1, 2, 3), VoxelBufferInternal::CHANNEL_SDF);
		} else {
			++successful_compiles_count;
		}
	}

	// print_line("--- End of zone with possible errors ---");
	print_line(String("Successful random compiles: {0}/{1}").format(varray(successful_compiles_count, attempts)));
}

void test_voxel_graph_sphere_on_plane() {
	static const float RADIUS = 6.f;
	struct L {
		static Ref<VoxelGeneratorGraph> create(bool debug) {
			Ref<VoxelGeneratorGraph> generator;
			generator.instantiate();
			load_graph_with_sphere_on_plane(**generator->get_main_function(), RADIUS);
			pg::CompilationResult compilation_result = generator->compile(debug);
			ZN_TEST_ASSERT_MSG(compilation_result.success,
					String("Failed to compile graph: {0}: {1}")
							.format(varray(compilation_result.node_id, compilation_result.message)));
			return generator;
		}

		static void test_locations(VoxelGeneratorGraph &g) {
			const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
			const float sd_sky_above_sphere = g.generate_single(Vector3i(0, RADIUS + 5, 0), channel).f;
			const float sd_sky_away_from_sphere = g.generate_single(Vector3i(100, RADIUS + 5, 0), channel).f;
			const float sd_ground_below_sphere = g.generate_single(Vector3i(0, -RADIUS - 5, 0), channel).f;
			const float sd_ground_away_from_sphere = g.generate_single(Vector3i(100, -RADIUS - 5, 0), channel).f;
			const float sd_at_sphere_center = g.generate_single(Vector3i(0, 0, 0), channel).f;
			const float sd_in_sphere_but_higher_than_center =
					g.generate_single(Vector3i(RADIUS / 2, RADIUS / 2, RADIUS / 2), channel).f;

			ZN_TEST_ASSERT(sd_sky_above_sphere > 0.f);
			ZN_TEST_ASSERT(sd_sky_away_from_sphere > 0.f);
			ZN_TEST_ASSERT(sd_ground_below_sphere < 0.f);
			ZN_TEST_ASSERT(sd_ground_away_from_sphere < 0.f);
			ZN_TEST_ASSERT(sd_at_sphere_center < 0.f);
			ZN_TEST_ASSERT(sd_in_sphere_but_higher_than_center < 0.f);
			ZN_TEST_ASSERT(sd_in_sphere_but_higher_than_center > sd_at_sphere_center);
		}
	};
	Ref<VoxelGeneratorGraph> generator_debug = L::create(true);
	Ref<VoxelGeneratorGraph> generator = L::create(false);
	ZN_ASSERT(check_graph_results_are_equal(**generator_debug, **generator));
	L::test_locations(**generator_debug);
	L::test_locations(**generator);
}

#ifdef VOXEL_ENABLE_FAST_NOISE_2

// https://github.com/Zylann/godot_voxel/issues/427
void test_voxel_graph_issue427() {
	Ref<VoxelGeneratorGraph> graph;
	graph.instantiate();
	VoxelGraphFunction &g = **graph->get_main_function();

	const uint32_t n_in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2()); // 1
	const uint32_t n_sub = g.create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2()); // 2
	const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2()); // 3
	const uint32_t n_mul = g.create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2()); // 4
	const uint32_t n_fn2_2d = g.create_node(VoxelGraphFunction::NODE_FAST_NOISE_2_2D, Vector2()); // 5
	const uint32_t n_distance_3d = g.create_node(VoxelGraphFunction::NODE_DISTANCE_3D, Vector2()); // 6

	g.add_connection(n_in_y, 0, n_sub, 0);
	g.add_connection(n_sub, 0, n_out_sdf, 0);
	g.add_connection(n_fn2_2d, 0, n_mul, 0);
	g.add_connection(n_distance_3d, 0, n_mul, 1);
	// Was crashing after adding this connection
	g.add_connection(n_mul, 0, n_sub, 1);

	pg::CompilationResult result = graph->compile(true);
	ZN_TEST_ASSERT(result.success);
}

#ifdef TOOLS_ENABLED

void test_voxel_graph_hash() {
	Ref<VoxelGraphFunction> graph;
	graph.instantiate();
	VoxelGraphFunction &g = **graph;

	const uint32_t n_in_y = g.create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2()); // 1
	const uint32_t n_add = g.create_node(VoxelGraphFunction::NODE_ADD, Vector2()); // 2
	const uint32_t n_mul = g.create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2()); // 3
	const uint32_t n_out_sdf = g.create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2()); // 4
	const uint32_t n_fn2_2d = g.create_node(VoxelGraphFunction::NODE_FAST_NOISE_2_2D, Vector2()); // 5

	// Initial hash
	const uint64_t hash0 = g.get_output_graph_hash();

	// Setting a default input on a node that isn't connected yet to the output
	g.set_node_default_input(n_mul, 1, 2);
	const uint64_t hash1 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash1 == hash0);

	// Adding connections up to the output
	g.add_connection(n_in_y, 0, n_add, 0);
	g.add_connection(n_fn2_2d, 0, n_add, 1);
	g.add_connection(n_add, 0, n_mul, 0);
	g.add_connection(n_mul, 0, n_out_sdf, 0);
	const uint64_t hash2 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash2 != hash0);

	// Adding only one connection, creating a diamond
	g.add_connection(n_fn2_2d, 0, n_mul, 1);
	const uint64_t hash3 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash3 != hash2);

	// Setting a default input
	g.set_node_default_input(n_mul, 1, 4);
	const uint64_t hash4 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash4 != hash3);

	// Setting a noise resource property
	Ref<FastNoise2> noise = g.get_node_param(n_fn2_2d, 0);
	noise->set_period(noise->get_period() + 10.f);
	const uint64_t hash5 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash5 != hash4);

	// Setting a different noise instance with the same properties
	Ref<FastNoise2> noise2 = noise->duplicate();
	g.set_node_param(n_fn2_2d, 0, noise2);
	const uint64_t hash6 = g.get_output_graph_hash();
	ZN_TEST_ASSERT(hash6 == hash5);
}

#endif // TOOLS_ENABLED
#endif // VOXEL_ENABLE_FAST_NOISE_2

void test_voxel_graph_issue471() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	Ref<VoxelGraphFunction> func = generator->get_main_function();
	ZN_ASSERT(func.is_valid());
	FixedArray<VoxelGraphFunction::Port, 1> inputs;
	inputs[0].name = "test_input";
	inputs[0].type = VoxelGraphFunction::NODE_INPUT_X;
	FixedArray<VoxelGraphFunction::Port, 1> outputs;
	outputs[0].name = "test_output";
	outputs[0].type = VoxelGraphFunction::NODE_OUTPUT_SDF;
	func->set_io_definitions(to_span(inputs), to_span(outputs));
	// Was crashing because input definition wasn't fulfilled (the graph is empty). It should fail with an error.
	VoxelGenerator::ShaderSourceData ssd;
	generator->get_shader_source(ssd);
}

// There was a bug where generating a usual height-based terrain with also a texture output, random blocks fully or
// partially filled with air would occur underground where such blocks should have been filled with matter. It only
// happened if the texture output node was present. The cause was that the generator detected and filled the SDF early
// with matter, for blocks far enough from the surface. But because there was also a texture output, the generator
// proceeded to still run the graph to just get volumetric texture data (which is expected) but then overwrote SDF with
// results it did not calculate, effectively filling SDF with garbage.
void test_voxel_graph_unused_single_texture_output() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();

	{
		//               Plane
		//                    \
		// Noise2D --- Mul --- Sub --- OutSDF
		//
		//                             OutSingleTexture

		// Slightly bumpy ground around Y=0, not going higher than 10 or lower than -10 voxels.

		Ref<VoxelGraphFunction> func = generator->get_main_function();
		ZN_ASSERT(func.is_valid());

		const uint32_t n_out_sdf = func->create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		const uint32_t n_plane = func->create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2());

		const uint32_t n_noise = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2());
		Ref<ZN_FastNoiseLite> fnl;
		fnl.instantiate();
		fnl->set_period(1024);
		fnl->set_fractal_type(ZN_FastNoiseLite::FRACTAL_RIDGED);
		fnl->set_fractal_octaves(5);
		func->set_node_param(n_noise, 0, fnl);

		const uint32_t n_mul = func->create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2());
		func->set_node_default_input(n_mul, 1, 10.0);

		const uint32_t n_sub = func->create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2());

		const uint32_t n_out_single_texture =
				func->create_node(VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE, Vector2());

		func->add_connection(n_plane, 0, n_sub, 0);
		func->add_connection(n_noise, 0, n_mul, 0);
		func->add_connection(n_mul, 0, n_sub, 1);
		func->add_connection(n_sub, 0, n_out_sdf, 0);
	}

	CompilationResult result = generator->compile(false);
	ZN_TEST_ASSERT(result.success);

	std::vector<Vector3i> block_positions;
	{
		Vector3i bpos;
		for (bpos.z = -4; bpos.z < 4; ++bpos.z) {
			for (bpos.x = -4; bpos.x < 4; ++bpos.x) {
				for (bpos.y = -4; bpos.y < 4; ++bpos.y) {
					block_positions.push_back(bpos);
				}
			}
		}

		struct Comparer {
			inline bool operator()(const Vector3i a, const Vector3i b) const {
				return math::length(to_vec3f(a)) < math::length(to_vec3f(b));
			}
		};
		SortArray<Vector3i, Comparer> sorter;
		sorter.sort(block_positions.data(), block_positions.size());
	}

	VoxelBufferInternal voxels;
	const int BLOCK_SIZE = 16;
	const int MIN_MARGIN = 1;
	const int MAX_MARGIN = 2;
	voxels.create(Vector3iUtil::create(BLOCK_SIZE + MIN_MARGIN + MAX_MARGIN));

	for (const Vector3i bpos : block_positions) {
		const Vector3i origin_in_voxels = bpos * BLOCK_SIZE - Vector3iUtil::create(MIN_MARGIN);
		generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels, origin_in_voxels, 0 });

		if (bpos.y <= -2) {
			// We expect only ground below this height (in block coordinates)
			for (int z = 0; z < voxels.get_size().z; ++z) {
				for (int x = 0; x < voxels.get_size().x; ++x) {
					for (int y = 0; y < voxels.get_size().y; ++y) {
						const float sd = voxels.get_voxel_f(x, y, z, VoxelBufferInternal::CHANNEL_SDF);

						if (sd >= 0.f) {
							print_sdf_as_ascii(voxels);
						}

						ZN_TEST_ASSERT(sd < 0.f);
					}
				}
			}
		} else if (bpos.y >= 1) {
			// We expect only air above this height (in block coordinates)
			for (int z = 0; z < voxels.get_size().z; ++z) {
				for (int x = 0; x < voxels.get_size().x; ++x) {
					for (int y = 0; y < voxels.get_size().y; ++y) {
						const float sd = voxels.get_voxel_f(x, y, z, VoxelBufferInternal::CHANNEL_SDF);
						ZN_TEST_ASSERT(sd > 0.f);
					}
				}
			}
		}
	}
}

// There was a bug where texture indices selected using a Spots2D node were returning garbage in areas that were
// supposed to be optimized out. The bug doesn't happen if local execution map optimization is turned off. In those
// areas, spots aren't present: range analysis finds Spots2D always returns 0, which means Select ignores it and outputs
// a constant. But instead, it appears as if it returned the last values obtained in an area where a spot was present.
void test_voxel_graph_spots2d_optimized_execution_map() {
	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();

	const float SPOT_RADIUS = 5.f;
	const float CELL_SIZE = 64.f;
	const float JITTER = 0.f; // All spots are centered in their cell
	const unsigned int TEX_INDEX0 = 0;
	const unsigned int TEX_INDEX1 = 1;

	{
		// generator = ResourceLoader::load("res://local_tests/smooth_materials/smooth_materials_generator_graph.tres");

		Ref<VoxelGraphFunction> func = generator->get_main_function();
		ZN_ASSERT(func.is_valid());

		const uint32_t n4_out_sdf = func->create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2(), 4);

		const uint32_t n5_plane = func->create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2(), 5);
		uint32_t height_input_index;
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_input_index_from_name(
				VoxelGraphFunction::NODE_SDF_PLANE, "height", height_input_index));
		func->set_node_default_input(n5_plane, height_input_index, 2.f);

		const uint32_t n6_fnl1 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 6);

		const uint32_t n7_mul = func->create_node(VoxelGraphFunction::NODE_MULTIPLY, Vector2(), 7);
		func->set_node_default_input(n7_mul, 1, 1.f); // b

		const uint32_t n9_sub = func->create_node(VoxelGraphFunction::NODE_SUBTRACT, Vector2(), 9);
		const uint32_t n11_fnl2 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 11);
		const uint32_t n12_out_tex = func->create_node(VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE, Vector2(), 12);

		const uint32_t n13_select1 = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2(), 13);
		func->set_node_default_input(n13_select1, 0, 0.f); // a
		func->set_node_default_input(n13_select1, 1, 1.f); // b
		func->set_node_param(n13_select1, 0, 0.5f); // threshold

		const uint32_t n14_fnl3 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 14);

		const uint32_t n15_select2 = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2(), 15);
		func->set_node_default_input(n15_select2, 0, 0.f); // a
		func->set_node_default_input(n15_select2, 1, 2.f); // b
		func->set_node_param(n15_select2, 0, 0.5f); // threshold

		const uint32_t n16_fnl4 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 16);

		const uint32_t n18_select3 = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2(), 18);
		func->set_node_default_input(n18_select3, 0, 0.f); // a
		func->set_node_default_input(n18_select3, 1, 3.f); // b
		func->set_node_param(n18_select3, 0, 0.5f); // threshold

		const uint32_t n19_select4 = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2(), 19);
		func->set_node_default_input(n19_select4, 0, 0.f); // a
		func->set_node_default_input(n19_select4, 1, 4.f); // b
		func->set_node_param(n19_select4, 0, 0.5f); // threshold

		const uint32_t n20_fnl5 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 20);
		const uint32_t n21_fnl6 = func->create_node(VoxelGraphFunction::NODE_FAST_NOISE_2D, Vector2(), 21);

		const uint32_t n22_select5 = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2(), 22);
		func->set_node_default_input(n22_select5, 0, 0.f); // a
		func->set_node_default_input(n22_select5, 1, 5.f); // b
		func->set_node_param(n22_select5, 0, 0.5f); // threshold

		const uint32_t n23_spots2d = func->create_node(VoxelGraphFunction::NODE_SPOTS_2D, Vector2(), 23);
		uint32_t cell_size_param_index;
		uint32_t jitter_param_index;
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_param_index_from_name(
				VoxelGraphFunction::NODE_SPOTS_2D, "cell_size", cell_size_param_index));
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_param_index_from_name(
				VoxelGraphFunction::NODE_SPOTS_2D, "jitter", jitter_param_index));
		func->set_node_param(n23_spots2d, cell_size_param_index, CELL_SIZE);
		func->set_node_param(n23_spots2d, jitter_param_index, JITTER);
		func->set_node_default_input(n23_spots2d, 2, SPOT_RADIUS);

		func->add_connection(n19_select4, 0, n22_select5, 0);
		func->add_connection(n13_select1, 0, n12_out_tex, 0);
		func->add_connection(n14_fnl3, 0, n15_select2, 2);
		func->add_connection(n15_select2, 0, n18_select3, 0);
		func->add_connection(n16_fnl4, 0, n18_select3, 2);
		func->add_connection(n18_select3, 0, n19_select4, 0);
		func->add_connection(n20_fnl5, 0, n19_select4, 2);
		func->add_connection(n21_fnl6, 0, n22_select5, 2);
		func->add_connection(n23_spots2d, 0, n13_select1, 2);
		func->add_connection(n5_plane, 0, n9_sub, 0);
		func->add_connection(n6_fnl1, 0, n7_mul, 0);
		func->add_connection(n7_mul, 0, n9_sub, 1);
		func->add_connection(n9_sub, 0, n4_out_sdf, 0);

		/*
		// Plane --- OutSDF
		//
		//           0
		//            \
		//       1 --- Select --- OutSingleTexture
		//            /
		//     Spots2D
		//
		// Flat terrain with spots

		Ref<VoxelGraphFunction> func = generator->get_main_function();
		ZN_ASSERT(func.is_valid());

		const uint32_t n_out_sdf = func->create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());
		const uint32_t n_plane = func->create_node(VoxelGraphFunction::NODE_SDF_PLANE, Vector2());
		const uint32_t n_select = func->create_node(VoxelGraphFunction::NODE_SELECT, Vector2());
		const uint32_t n_spots2d = func->create_node(VoxelGraphFunction::NODE_SPOTS_2D, Vector2());
		const uint32_t n_out_single_texture =
				func->create_node(VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE, Vector2());

		uint32_t height_input_index;
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_input_index_from_name(
				VoxelGraphFunction::NODE_SDF_PLANE, "height", height_input_index));
		func->set_node_default_input(n_plane, height_input_index, 1.f);

		uint32_t cell_size_param_index;
		uint32_t jitter_param_index;
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_param_index_from_name(
				VoxelGraphFunction::NODE_SPOTS_2D, "cell_size", cell_size_param_index));
		ZN_ASSERT(pg::NodeTypeDB::get_singleton().try_get_param_index_from_name(
				VoxelGraphFunction::NODE_SPOTS_2D, "jitter", jitter_param_index));
		func->set_node_param(n_spots2d, cell_size_param_index, CELL_SIZE);
		func->set_node_param(n_spots2d, jitter_param_index, JITTER);
		func->set_node_default_input(n_spots2d, 2, SPOT_RADIUS);

		func->set_node_default_input(n_select, 0, TEX_INDEX0);
		func->set_node_default_input(n_select, 1, TEX_INDEX1);
		func->set_node_param(n_select, 0, 0.5f); // Threshold

		func->add_connection(n_plane, 0, n_out_sdf, 0);
		func->add_connection(n_spots2d, 0, n_select, 2);
		func->add_connection(n_select, 0, n_out_single_texture, 0);
		//*/
	}

	CompilationResult result = generator->compile(false);
	ZN_TEST_ASSERT(result.success);

	struct L {
		static bool has_spot(const VoxelBufferInternal &vb) {
			Vector3i pos;
			for (pos.z = 0; pos.z < vb.get_size().z; ++pos.z) {
				for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
					for (pos.y = 0; pos.y < vb.get_size().y; ++pos.y) {
						const uint32_t encoded_indices = vb.get_voxel(pos, VoxelBufferInternal::CHANNEL_INDICES);
						const uint32_t encoded_weights = vb.get_voxel(pos, VoxelBufferInternal::CHANNEL_WEIGHTS);
						const FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(encoded_indices);
						const FixedArray<uint8_t, 4> weights = decode_weights_from_packed_u16(encoded_weights);
						int indices_with_high_weight = 0;
						bool has_tex1 = false;
						for (unsigned int i = 0; i < 4; ++i) {
							if (weights[i] > 200) {
								const uint8_t ii = indices[i];
								ZN_TEST_ASSERT_MSG(ii == TEX_INDEX0 || ii == TEX_INDEX1,
										"Expected only one of our two indices with high weight");
								++indices_with_high_weight;
								if (ii == TEX_INDEX1) {
									has_tex1 = true;
								}
							}
						}
						ZN_TEST_ASSERT(indices_with_high_weight == 1);
						if (has_tex1) {
							return true;
						}
					}
				}
			}
			return false;
		}

		static std::string print_u16_hex(uint16_t x) {
			const char *s_chars = "0123456789abcdef";
			std::string s;
			for (int i = 3; i >= 0; --i) {
				const unsigned int nibble = (x >> (i * 4)) & 0xf;
				s += s_chars[nibble];
			}
			return s;
		}

		static void print_indices_and_weights(const VoxelBufferInternal &vb, int y) {
			Vector3i pos(0, y, 0);
			std::string s;
			for (pos.z = 0; pos.z < vb.get_size().z; ++pos.z) {
				for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
					const uint16_t encoded_indices = vb.get_voxel(pos, VoxelBufferInternal::CHANNEL_INDICES);
					s += print_u16_hex(encoded_indices);
					s += " ";
				}
				s += " | ";
				for (pos.x = 0; pos.x < vb.get_size().x; ++pos.x) {
					const uint16_t encoded_weights = vb.get_voxel(pos, VoxelBufferInternal::CHANNEL_WEIGHTS);
					s += print_u16_hex(encoded_weights);
					s += " ";
				}
				s += "\n";
			}
			println(format("Indices and weights at Y={}:", y));
			println(s);
		}
	};

	const int BLOCK_SIZE = 16;

	VoxelBufferInternal voxels1;
	voxels1.create(Vector3iUtil::create(BLOCK_SIZE));
	VoxelBufferInternal voxels2;
	voxels2.create(Vector3iUtil::create(BLOCK_SIZE));

	// First do a run without the optimization
	generator->set_use_optimized_execution_map(false);
	{
		// There is a spot in the top-right corner of this area
		generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels1, Vector3i(16, 0, 16), 0 });
		// L::print_indices_and_weights(voxels1, 8);
		ZN_TEST_ASSERT(L::has_spot(voxels1));

		// There is no spot here
		generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels2, Vector3i(0, 0, 0), 0 });
		// L::print_indices_and_weights(voxels2, 8);
		ZN_TEST_ASSERT(L::has_spot(voxels2) == false);
	}

	VoxelBufferInternal voxels3;
	voxels3.create(Vector3iUtil::create(BLOCK_SIZE));
	VoxelBufferInternal voxels4;
	voxels4.create(Vector3iUtil::create(BLOCK_SIZE));

	// Now do a run with the optimization, results must be the same
	generator->set_use_optimized_execution_map(true);
	{
		generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels3, Vector3i(16, 0, 16), 0 });
		// L::print_indices_and_weights(voxels3, 8);
		ZN_TEST_ASSERT(L::has_spot(voxels3));
		ZN_TEST_ASSERT(voxels3.equals(voxels1));

		generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels4, Vector3i(0, 0, 0), 0 });
		// L::print_indices_and_weights(voxels4, 8);
		ZN_TEST_ASSERT(L::has_spot(voxels4) == false);
		ZN_TEST_ASSERT(voxels4.equals(voxels2));
	}

	// Broader test
	/*{
		struct BlockTest {
			Vector3i origin;
			bool expect_spot;
		};

		std::vector<BlockTest> block_tests;

		generator->set_use_optimized_execution_map(false);

		Vector3i bpos;
		for (bpos.z = -4; bpos.z < 4; ++bpos.z) {
			for (bpos.x = -4; bpos.x < 4; ++bpos.x) {
				const Vector3i origin = bpos * BLOCK_SIZE;
				generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels, origin, 0 });
				block_tests.push_back(BlockTest{ origin, L::has_spot(voxels) });
			}
		}

		generator->set_use_optimized_execution_map(true);

		for (unsigned int bti = 0; bti < block_tests.size(); ++bti) {
			const BlockTest bt = block_tests[bti];
			generator->generate_block(VoxelGenerator::VoxelQueryData{ voxels, bt.origin, 0 });
			const bool spot_found = L::has_spot(voxels);
			ZN_TEST_ASSERT(bt.expect_spot == spot_found);
		}
	}*/
}

void test_voxel_graph_unused_inner_output() {
	// When compiling a graph with an unused output in one if its inner nodes (not an Output* node), compiling in debug
	// would crash because it tries to allocate an output buffer with 0 users, which should be allowed specifically in
	// debug. To reproduce this, we need to have a node with more than one output, and one output being being used for a
	// graph output. So the node will get compiled as part of the program, but will have an unused output. In non-debug
	// this output will be allocated as a temporary throwaway buffer, but in debug all outputs are allocated regardless
	// since buffer allocations are not optimized.

	Ref<VoxelGeneratorGraph> generator;
	generator.instantiate();
	{
		Ref<VoxelGraphFunction> g = generator->get_main_function();
		ZN_ASSERT(g.is_valid());

		//    X             OutSDF
		//     \           /
		// Y -- Normalize3D -- (unused `ny`)
		//     /         \ \
		//    Z           \ (unused `nz`)
		//                 \
		//                  (unused `len`)

		const uint32_t n_x = g->create_node(VoxelGraphFunction::NODE_INPUT_X, Vector2());
		const uint32_t n_y = g->create_node(VoxelGraphFunction::NODE_INPUT_Y, Vector2());
		const uint32_t n_z = g->create_node(VoxelGraphFunction::NODE_INPUT_Z, Vector2());
		const uint32_t n_normalize = g->create_node(VoxelGraphFunction::NODE_NORMALIZE_3D, Vector2());
		const uint32_t n_out = g->create_node(VoxelGraphFunction::NODE_OUTPUT_SDF, Vector2());

		g->add_connection(n_x, 0, n_normalize, 0);
		g->add_connection(n_y, 0, n_normalize, 1);
		g->add_connection(n_z, 0, n_normalize, 2);
		g->add_connection(n_normalize, 0, n_out, 0);
		// Leave outputs `ny`, `nz` and `len` unused
	}

	const CompilationResult result_debug = generator->compile(true);
	ZN_TEST_ASSERT(result_debug.success);

	const CompilationResult result_ndebug = generator->compile(true);
	ZN_TEST_ASSERT(result_ndebug.success);
}

} // namespace zylann::voxel::tests
