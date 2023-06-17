#include "../node_type_db.h"
#include "../voxel_graph_runtime.h"
#include "util.h"

namespace zylann::voxel::pg {

// Special case for division because we want to avoid NaNs caused by zeros
void do_division(Runtime::ProcessBufferContext &ctx) {
	const Runtime::Buffer &a = ctx.get_input(0);
	const Runtime::Buffer &b = ctx.get_input(1);
	Runtime::Buffer &out = ctx.get_output(0);
	const uint32_t buffer_size = out.size;

	if (a.is_constant || b.is_constant) {
		if (!b.is_constant) {
			const float c = a.constant_value;
			const float *v = b.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = v[i] == 0.f ? 0.f : c / v[i];
			}

		} else if (!a.is_constant) {
			if (b.constant_value == 0.f) {
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = 0.f;
				}
			} else {
				const float c = 1.f / b.constant_value;
				const float *v = a.data;
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = v[i] * c;
				}
			}
		} else {
			// Normally this case should have been optimized out at compile-time
			const float v = b.constant_value == 0.f ? 0.f : a.constant_value / b.constant_value;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = v;
			}
		}

	} else {
		for (uint32_t i = 0; i < buffer_size; ++i) {
			if (b.data[i] == 0.f) {
				out.data[i] = 0.f;
			} else {
				out.data[i] = a.data[i] / b.data[i];
			}
		}
	}
}

void register_math_ops_nodes(Span<NodeType> types) {
	using namespace math;

	{
		NodeType &t = types[VoxelGraphFunction::NODE_ADD];
		t.name = "Add";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.compile_func = nullptr;
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a + b; });
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, a + b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} + {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_SUBTRACT];
		t.name = "Subtract";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a - b; });
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, a - b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} - {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_MULTIPLY];
		t.name = "Multiply";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a * b; });
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			if (ctx.get_input_address(0) == ctx.get_input_address(1)) {
				// The two operands have the same source, we can optimize to a square function
				ctx.set_output(0, squared(a));
			} else {
				ctx.set_output(0, a * b);
			}
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} * {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_DIVIDE];
		t.name = "Divide";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 1.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = do_division;
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, a / b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} / {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
}

} // namespace zylann::voxel::pg
