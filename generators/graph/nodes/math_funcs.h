#include "../node_type_db.h"
#include "../voxel_graph_runtime.h"
#include "util.h"

namespace zylann::voxel::pg {

void register_math_func_nodes(Span<NodeType> types) {
	typedef Runtime::ProcessBufferContext ProcessBufferContext;
	typedef Runtime::RangeAnalysisContext RangeAnalysisContext;

	using namespace math;

	{
		NodeType &t = types[VoxelGraphFunction::NODE_SIN];
		t.name = "Sin";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) { //
			do_monop(ctx, [](float a) { return Math::sin(a); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, sin(a));
		};
		t.expression_func_name = "sin";
		t.expression_func = [](Span<const float> args) { //
			return Math::sin(args[0]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = sin({});\n", ctx.get_output_name(0), ctx.get_input_name(0));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_FLOOR];
		t.name = "Floor";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_monop(ctx, [](float a) { return Math::floor(a); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, floor(a));
		};
		t.expression_func_name = "floor";
		t.expression_func = [](Span<const float> args) { //
			return Math::floor(args[0]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = floor({});\n", ctx.get_output_name(0), ctx.get_input_name(0));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_ABS];
		t.name = "Abs";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) { //
			do_monop(ctx, [](float a) { return Math::abs(a); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, abs(a));
		};
		t.expression_func_name = "abs";
		t.expression_func = [](Span<const float> args) { //
			return Math::abs(args[0]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = abs({});\n", ctx.get_output_name(0), ctx.get_input_name(0));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_SQRT];
		t.name = "Sqrt";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) { //
			do_monop(ctx, [](float a) { return Math::sqrt(math::max(a, 0.f)); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, sqrt(a));
		};
		t.expression_func_name = "sqrt";
		t.expression_func = [](Span<const float> args) { //
			return Math::sqrt(math::max(args[0], 0.f));
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = sqrt(max({}, 0.0));\n", ctx.get_output_name(0), ctx.get_input_name(0));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_FRACT];
		t.name = "Fract";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_monop(ctx, [](float a) { return a - Math::floor(a); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a - floor(a));
		};
		t.expression_func_name = "fract";
		t.expression_func = [](Span<const float> args) { //
			return args[0] - Math::floor(args[0]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = fract({});\n", ctx.get_output_name(0), ctx.get_input_name(0));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_STEPIFY];
		t.name = "Stepify";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("step", 1.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return math::snappedf(a, b); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, snapped(a, b));
		};
		t.expression_func_name = "stepify";
		t.expression_func = [](Span<const float> args) { //
			return math::snappedf(args[0], args[1]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("stepify",
					"float vg_stepify(float value, float step) {\n"
					"	return floor(p_value / step + 0.5f) * step;\n"
					"}\n");
			ctx.add_format(
					"{} = vg_stepify({}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_WRAP];
		t.name = "Wrap";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("length", 1.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return wrapf(a, b); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, wrapf(a, b));
		};
		t.expression_func_name = "wrap";
		t.expression_func = [](Span<const float> args) { //
			return wrapf(args[0], args[1]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("wrap",
					"float vg_wrap(float x, float d) {\n"
					"	return x - (d * floor(x / d));\n"
					"}\n");
			ctx.add_format(
					"{} = vg_wrap({}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_MIN];
		t.name = "Min";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return min(a, b); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, min_interval(a, b));
		};
		t.expression_func_name = "min";
		t.expression_func = [](Span<const float> args) { //
			return min(args[0], args[1]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = min({}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_MAX];
		t.name = "Max";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return max(a, b); });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, max_interval(a, b));
		};
		t.expression_func_name = "max";
		t.expression_func = [](Span<const float> args) { //
			return max(args[0], args[1]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = max({}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_CLAMP];
		t.name = "Clamp";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x"));
		t.inputs.push_back(NodeType::Port("min", -1.f));
		t.inputs.push_back(NodeType::Port("max", 1.f));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &a = ctx.get_input(0);
			const Runtime::Buffer &minv = ctx.get_input(1);
			const Runtime::Buffer &maxv = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = clamp(a.data[i], minv.data[i], maxv.data[i]);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval minv = ctx.get_input(1);
			const Interval maxv = ctx.get_input(2);
			ctx.set_output(0, clamp(a, minv, maxv));
		};
		t.expression_func_name = "clamp";
		t.expression_func = [](Span<const float> args) { //
			return clamp(args[0], args[1], args[2]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = clamp({}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2));
		};
	}
	{
		struct Params {
			float min;
			float max;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_CLAMP_C];
		t.name = "ClampC";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x"));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("min", Variant::FLOAT, -1.f));
		t.params.push_back(NodeType::Param("max", Variant::FLOAT, 1.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.min = ctx.get_param(0).operator float();
			p.max = ctx.get_param(1).operator float();
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &a = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = clamp(a.data[i], p.min, p.max);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Params p = ctx.get_params<Params>();
			const Interval cmin = Interval::from_single_value(p.min);
			const Interval cmax = Interval::from_single_value(p.max);
			ctx.set_output(0, clamp(a, cmin, cmax));
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = clamp({}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					float(ctx.get_param(0)), float(ctx.get_param(1)));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_MIX];
		t.name = "Mix";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("a"));
		t.inputs.push_back(NodeType::Port("b"));
		t.inputs.push_back(NodeType::Port("ratio"));
		t.outputs.push_back(NodeType::Port("out"));
		// TODO Add a `clamp` parameter? It helps optimization
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			bool a_ignored;
			bool b_ignored;
			const Runtime::Buffer &a = ctx.try_get_input(0, a_ignored);
			const Runtime::Buffer &b = ctx.try_get_input(1, b_ignored);
			const Runtime::Buffer &r = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const uint32_t buffer_size = out.size;
			if (a.is_constant) {
				const float ca = a.constant_value;
				if (b.is_constant) {
					const float cb = b.constant_value;
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = Math::lerp(ca, cb, r.data[i]);
					}
				} else {
					if (b_ignored) {
						for (uint32_t i = 0; i < buffer_size; ++i) {
							out.data[i] = ca;
						}
					} else {
						for (uint32_t i = 0; i < buffer_size; ++i) {
							out.data[i] = Math::lerp(ca, b.data[i], r.data[i]);
						}
					}
				}
			} else if (b.is_constant) {
				const float cb = b.constant_value;
				if (a_ignored) {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = cb;
					}
				} else {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = Math::lerp(a.data[i], cb, r.data[i]);
					}
				}
			} else {
				if (a_ignored) {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = b.data[i];
					}
				} else if (b_ignored) {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = a.data[i];
					}
				} else {
					for (uint32_t i = 0; i < buffer_size; ++i) {
						out.data[i] = Math::lerp(a.data[i], b.data[i], r.data[i]);
					}
				}
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			// Note: if I call this `t` like I use to do with `lerp`, GCC complains it shadows `t` from the outer scope.
			// Even though this lambda does not capture anything from the outer scope :shrug:
			const Interval r = ctx.get_input(2);
			if (r.is_single_value()) {
				if (r.min == 1.f) {
					// a will be ignored
					ctx.ignore_input(0);
				} else if (r.min == 0.f) {
					// b will be ignored
					ctx.ignore_input(1);
				}
			}
			ctx.set_output(0, lerp(a, b, r));
		};
		t.expression_func_name = "lerp";
		t.expression_func = [](Span<const float> args) { //
			return Math::lerp(args[0], args[1], args[2]);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = mix({}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2));
		};
	}
	{
		struct Params {
			// Remap can be reduced to a linear function
			// a * x + b
			float a;
			float b;

			static Params from_intervals(float min0, float max0, float min1, float max1) {
				Params p;
				math::remap_intervals_to_linear_params(min0, max0, min1, max1, p.a, p.b);
				return p;
			}
		};
		NodeType &t = types[VoxelGraphFunction::NODE_REMAP];
		t.name = "Remap";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x"));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("min0", Variant::FLOAT, -1.f));
		t.params.push_back(NodeType::Param("max0", Variant::FLOAT, 1.f));
		t.params.push_back(NodeType::Param("min1", Variant::FLOAT, -1.f));
		t.params.push_back(NodeType::Param("max1", Variant::FLOAT, 1.f));
		t.compile_func = [](CompileContext &ctx) {
			const float min0 = ctx.get_param(0).operator float();
			const float max0 = ctx.get_param(1).operator float();
			const float min1 = ctx.get_param(2).operator float();
			const float max1 = ctx.get_param(3).operator float();
			ctx.set_params(Params::from_intervals(min0, max0, min1, max1));
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.a * x.data[i] + p.b;
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Params p = ctx.get_params<Params>();
			ctx.set_output(0, p.a * x + p.b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			const Params p = Params::from_intervals(
					float(ctx.get_param(0)), float(ctx.get_param(1)), float(ctx.get_param(2)), float(ctx.get_param(3)));
			ctx.add_format("{} = {} * {} + {};\n", ctx.get_output_name(0), p.a, ctx.get_input_name(0), p.b);
		};
	}
	{
		struct Params {
			float edge0;
			float edge1;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SMOOTHSTEP];
		t.name = "Smoothstep";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x"));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("edge0", Variant::FLOAT, 0.f));
		t.params.push_back(NodeType::Param("edge1", Variant::FLOAT, 1.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.edge0 = ctx.get_param(0).operator float();
			p.edge1 = ctx.get_param(1).operator float();
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &a = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = smoothstep(p.edge0, p.edge1, a.data[i]);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Params p = ctx.get_params<Params>();
			ctx.set_output(0, smoothstep(p.edge0, p.edge1, a));
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = smoothstep({}, {}, {});\n", ctx.get_output_name(0), float(ctx.get_param(0)),
					float(ctx.get_param(1)), ctx.get_input_name(0));
		};
	}
	{
		struct Params {
			unsigned int power;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_POWI];
		t.name = "Powi";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x"));
		t.params.push_back(NodeType::Param("power", Variant::INT, 2));
		t.outputs.push_back(NodeType::Port("out"));

		t.compile_func = [](CompileContext &ctx) {
			const int power = ctx.get_param(0).operator int();
			if (power < 0) {
				ctx.make_error(ZN_TTR("Power cannot be negative"));
			} else {
				Params p;
				p.power = power;
				ctx.set_params(p);
			}
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const unsigned int power = ctx.get_params<Params>().power;
			switch (power) {
				case 0:
					for (unsigned int i = 0; i < out.size; ++i) {
						out.data[i] = 1.f;
					}
					break;
				case 1:
					for (unsigned int i = 0; i < out.size; ++i) {
						out.data[i] = x.data[i];
					}
					break;
				default:
					for (unsigned int i = 0; i < out.size; ++i) {
						float v = x.data[i];
						for (unsigned int p = 1; p < power; ++p) {
							v *= v;
						}
						out.data[i] = v;
					}
					break;
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const unsigned int power = ctx.get_params<Params>().power;
			ctx.set_output(0, powi(x, power));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			const int power = ctx.get_param(0).operator int();
			if (power < 0) {
				ctx.make_error(ZN_TTR("Power cannot be negative"));
			}
			ctx.add_format("{} = 1.0;\n", ctx.get_output_name(0));
			for (int i = 0; i < power; ++i) {
				ctx.add_format("{} *= {};\n", ctx.get_output_name(0), ctx.get_input_name(0));
			}
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_POW];
		t.name = "Pow";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x"));
		t.inputs.push_back(NodeType::Port("p", 2.f));
		t.outputs.push_back(NodeType::Port("out"));

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &p = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = Math::pow(x.data[i], p.data[i]);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			ctx.set_output(0, pow(x, y));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = pow({}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
}

} // namespace zylann::voxel::pg
