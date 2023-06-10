#include "../../../util/profiling.h"
#include "../node_type_db.h"

namespace zylann::voxel::pg {

void register_math_vector_nodes(Span<NodeType> types) {
	using namespace math;

	{
		NodeType &t = types[VoxelGraphFunction::NODE_DISTANCE_2D];
		t.name = "Distance2D";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x0"));
		t.inputs.push_back(NodeType::Port("y0"));
		t.inputs.push_back(NodeType::Port("x1", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y1", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x0 = ctx.get_input(0);
			const Runtime::Buffer &y0 = ctx.get_input(1);
			const Runtime::Buffer &x1 = ctx.get_input(2);
			const Runtime::Buffer &y1 = ctx.get_input(3);
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = Math::sqrt(squared(x1.data[i] - x0.data[i]) + squared(y1.data[i] - y0.data[i]));
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x0 = ctx.get_input(0);
			const Interval y0 = ctx.get_input(1);
			const Interval x1 = ctx.get_input(2);
			const Interval y1 = ctx.get_input(3);
			const Interval dx = x1 - x0;
			const Interval dy = y1 - y0;
			const Interval r = sqrt(squared(dx) + squared(dy));
			ctx.set_output(0, r);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = distance(vec2({}, {}), vec2({}, {}));\n", ctx.get_output_name(0),
					ctx.get_input_name(0), ctx.get_input_name(1), ctx.get_input_name(2), ctx.get_input_name(3));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_DISTANCE_3D];
		t.name = "Distance3D";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x0"));
		t.inputs.push_back(NodeType::Port("y0"));
		t.inputs.push_back(NodeType::Port("z0"));
		t.inputs.push_back(NodeType::Port("x1", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y1", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z1", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x0 = ctx.get_input(0);
			const Runtime::Buffer &y0 = ctx.get_input(1);
			const Runtime::Buffer &z0 = ctx.get_input(2);
			const Runtime::Buffer &x1 = ctx.get_input(3);
			const Runtime::Buffer &y1 = ctx.get_input(4);
			const Runtime::Buffer &z1 = ctx.get_input(5);
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = Math::sqrt(squared(x1.data[i] - x0.data[i]) + squared(y1.data[i] - y0.data[i]) +
						squared(z1.data[i] - z0.data[i]));
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x0 = ctx.get_input(0);
			const Interval y0 = ctx.get_input(1);
			const Interval z0 = ctx.get_input(2);
			const Interval x1 = ctx.get_input(3);
			const Interval y1 = ctx.get_input(4);
			const Interval z1 = ctx.get_input(5);
			const Interval dx = x1 - x0;
			const Interval dy = y1 - y0;
			const Interval dz = z1 - z0;
			Interval r = get_length(dx, dy, dz);
			ctx.set_output(0, r);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = distance(vec3({}, {}, {}), vec2({}, {}, {}));\n", ctx.get_output_name(0),
					ctx.get_input_name(0), ctx.get_input_name(1), ctx.get_input_name(2), ctx.get_input_name(3),
					ctx.get_input_name(4), ctx.get_input_name(5));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_NORMALIZE_3D];
		t.name = "Normalize";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 1.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 1.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 1.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("nx"));
		t.outputs.push_back(NodeType::Port("ny"));
		t.outputs.push_back(NodeType::Port("nz"));
		t.outputs.push_back(NodeType::Port("len"));

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_NORMALIZE_3D");
			const Runtime::Buffer &xb = ctx.get_input(0);
			const Runtime::Buffer &yb = ctx.get_input(1);
			const Runtime::Buffer &zb = ctx.get_input(2);
			Runtime::Buffer &out_nx = ctx.get_output(0);
			Runtime::Buffer &out_ny = ctx.get_output(1);
			Runtime::Buffer &out_nz = ctx.get_output(2);
			Runtime::Buffer &out_len = ctx.get_output(3);
			const uint32_t buffer_size = out_nx.size;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				const float x = xb.data[i];
				const float y = yb.data[i];
				const float z = zb.data[i];
				const float len = Math::sqrt(squared(x) + squared(y) + squared(z));
				out_nx.data[i] = x / len;
				out_ny.data[i] = y / len;
				out_nz.data[i] = z / len;
				out_len.data[i] = len;
			}
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Interval len = get_length(x, y, z);
			const Interval nx = x / len;
			const Interval ny = y / len;
			const Interval nz = z / len;
			ctx.set_output(0, nx);
			ctx.set_output(1, ny);
			ctx.set_output(2, nz);
			ctx.set_output(3, len);
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("vg_normalize",
					"void vg_normalize(vec3 v, out float x, out float y, out float z, out float mag) {\n"
					"    mag = length(v);\n"
					"    v /= mag;\n"
					"    x = v.x;\n"
					"    y = v.y;\n"
					"    z = v.z;\n"
					"}\n");
			ctx.add_format("vg_normalize(vec3({}, {}, {}), {}, {}, {}, {});\n", ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2), ctx.get_output_name(0), ctx.get_output_name(1),
					ctx.get_output_name(2), ctx.get_output_name(3));
		};
	}
}

} // namespace zylann::voxel::pg
