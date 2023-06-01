#include "node_type_db.h"
#include "../../constants/voxel_constants.h"
#include "../../engine/compute_shader_resource.h"
#include "../../util/godot/classes/curve.h"
#include "../../util/godot/classes/fast_noise_lite.h"
#include "../../util/godot/classes/image.h"
#include "../../util/godot/core/array.h"
#include "../../util/macros.h"
#include "../../util/math/sdf.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_range.h"
#include "../../util/noise/gd_noise_range.h"
#include "../../util/noise/spot_noise.h"
#include "../../util/profiling.h"
#include "../../util/string_funcs.h"
#include "fast_noise_lite_gdshader.h"
#include "image_range_grid.h"
#include "range_utility.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../../util/noise/fast_noise_2.h"
#endif

namespace zylann::voxel::pg {

namespace {
NodeTypeDB *g_node_type_db = nullptr;
}

using namespace math;

template <typename F>
inline void do_monop(pg::Runtime::ProcessBufferContext &ctx, F f) {
	const Runtime::Buffer &a = ctx.get_input(0);
	Runtime::Buffer &out = ctx.get_output(0);
	if (a.is_constant) {
		// Normally this case should have been optimized out at compile-time
		const float v = f(a.constant_value);
		for (uint32_t i = 0; i < a.size; ++i) {
			out.data[i] = v;
		}
	} else {
		for (uint32_t i = 0; i < a.size; ++i) {
			out.data[i] = f(a.data[i]);
		}
	}
}

template <typename F>
inline void do_binop(pg::Runtime::ProcessBufferContext &ctx, F f) {
	const Runtime::Buffer &a = ctx.get_input(0);
	const Runtime::Buffer &b = ctx.get_input(1);
	Runtime::Buffer &out = ctx.get_output(0);
	const uint32_t buffer_size = out.size;

	if (a.is_constant || b.is_constant) {
		if (!b.is_constant) {
			const float c = a.constant_value;
			const float *v = b.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(c, v[i]);
			}

		} else if (!a.is_constant) {
			const float c = b.constant_value;
			const float *v = a.data;
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = f(v[i], c);
			}

		} else {
			// Normally this case should have been optimized out at compile-time
			const float c = f(a.constant_value, b.constant_value);
			for (uint32_t i = 0; i < buffer_size; ++i) {
				out.data[i] = c;
			}
		}

	} else {
		for (uint32_t i = 0; i < buffer_size; ++i) {
			out.data[i] = f(a.data[i], b.data[i]);
		}
	}
}

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

inline float get_pixel_repeat(const Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

inline float get_pixel_repeat_linear(const Image &im, float x, float y) {
	const int x0 = int(Math::floor(x));
	const int y0 = int(Math::floor(y));

	const float xf = x - x0;
	const float yf = y - y0;

	const float h00 = get_pixel_repeat(im, x0, y0);
	const float h10 = get_pixel_repeat(im, x0 + 1, y0);
	const float h01 = get_pixel_repeat(im, x0, y0 + 1);
	const float h11 = get_pixel_repeat(im, x0 + 1, y0 + 1);

	// Bilinear filter
	const float h = Math::lerp(Math::lerp(h00, h10, xf), Math::lerp(h01, h11, xf), yf);

	return h;
}

inline float select(float a, float b, float threshold, float t) {
	return t < threshold ? a : b;
}

// inline Interval select(const Interval &a, const Interval &b, const Interval &threshold, const Interval &t) {
// 	if (t.max < threshold.min) {
// 		return a;
// 	}
// 	if (t.min >= threshold.max) {
// 		return b;
// 	}
// 	return Interval(min(a.min, b.min), max(a.max, b.max));
// }

inline float skew3(float x) {
	return (x * x * x + x) * 0.5f;
}

inline Interval skew3(Interval x) {
	return (cubed(x) + x) * 0.5f;
}

// This is mostly useful for generating planets from an existing heightmap
inline float sdf_sphere_heightmap(float x, float y, float z, float r, float m, const Image &im, float min_h,
		float max_h, float norm_x, float norm_y) {
	const float d = Math::sqrt(x * x + y * y + z * z) + 0.0001f;
	const float sd = d - r;
	// Optimize when far enough from heightmap.
	// This introduces a discontinuity but it should be ok for clamped storage
	const float margin = 1.2f * (max_h - min_h);
	if (sd > max_h + margin || sd < min_h - margin) {
		return sd;
	}
	const float nx = x / d;
	const float ny = y / d;
	const float nz = z / d;
	// TODO Could use fast atan2, it doesn't have to be precise
	// https://github.com/ducha-aiki/fast_atan2/blob/master/fast_atan.cpp
	const float uvx = -Math::atan2(nz, nx) * constants::INV_TAU + 0.5f;
	// This is an approximation of asin(ny)/(PI/2)
	// TODO It may be desirable to use the real function though,
	// in cases where we want to combine the same map in shaders
	const float ys = skew3(ny);
	const float uvy = -0.5f * ys + 0.5f;
	// TODO Could use bicubic interpolation when the image is sampled at lower resolution than voxels
	const float h = get_pixel_repeat_linear(im, uvx * norm_x, uvy * norm_y);
	return sd - m * h;
}

inline Interval sdf_sphere_heightmap(Interval x, Interval y, Interval z, float r, float m,
		const ImageRangeGrid *im_range, float norm_x, float norm_y) {
	const Interval d = get_length(x, y, z) + 0.0001f;
	const Interval sd = d - r;
	// TODO There is a discontinuity here due to the optimization done in the regular function
	// Not sure yet how to implement it here. Worst case scenario, we remove it

	const Interval nx = x / d;
	const Interval ny = y / d;
	const Interval nz = z / d;

	const Interval ys = skew3(ny);
	const Interval uvy = -0.5f * ys + 0.5f;

	// atan2 returns results between -PI and PI but sometimes the angle can wrap, we have to account for this
	OptionalInterval atan_r1;
	const Interval atan_r0 = atan2(nz, nx, &atan_r1);

	Interval h;
	{
		const Interval uvx = -atan_r0 * constants::INV_TAU + 0.5f;
		h = im_range->get_range(uvx * norm_x, uvy * norm_y);
	}
	if (atan_r1.valid) {
		const Interval uvx = -atan_r1.value * constants::INV_TAU + 0.5f;
		h.add_interval(im_range->get_range(uvx * norm_x, uvy * norm_y));
	}

	return sd - m * h;
}

template <typename T>
Variant create_resource_to_variant() {
	Ref<T> res;
	res.instantiate();
	return Variant(res);
}

const NodeTypeDB &NodeTypeDB::get_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	return *g_node_type_db;
}

void NodeTypeDB::create_singleton() {
	CRASH_COND(g_node_type_db != nullptr);
	g_node_type_db = memnew(NodeTypeDB());
}

void NodeTypeDB::destroy_singleton() {
	CRASH_COND(g_node_type_db == nullptr);
	memdelete(g_node_type_db);
	g_node_type_db = nullptr;
}

const char *get_category_name(Category category) {
	switch (category) {
		case CATEGORY_INPUT:
			return "Input";
		case CATEGORY_OUTPUT:
			return "Output";
		case CATEGORY_MATH:
			return "Math";
		case CATEGORY_CONVERT:
			return "Convert";
		case CATEGORY_GENERATE:
			return "Generate";
		case CATEGORY_SDF:
			return "Sdf";
		case CATEGORY_DEBUG:
			return "Debug";
		case CATEGORY_FUNCTIONS:
			return "Functions";
		case CATEGORY_RELAY:
			return "Relay";
		case CATEGORY_CONSTANT:
			return "Constant";
		default:
			CRASH_NOW_MSG("Unhandled category");
	}
	return "";
}

void add_fast_noise_lite_state_config(ShaderGenContext &ctx, const FastNoiseLite &fnl) {
	// TODO Add missing options
	ctx.add_format("fnl_state state = fnlCreateState({});\n"
				   "state.noise_type = {};\n"
				   "state.fractal_type = {};\n"
				   "state.octaves = {};\n"
				   "state.gain = {};\n"
				   "state.frequency = {};\n"
				   "state.lacunarity = {};\n",
			fnl.get_seed(), fnl.get_noise_type(), fnl.get_fractal_type(), fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(), fnl.get_frequency(), fnl.get_fractal_lacunarity());
}

void add_fast_noise_lite_state_config(ShaderGenContext &ctx, const ZN_FastNoiseLite &fnl) {
	// TODO Add missing options
	ctx.add_format("fnl_state state = fnlCreateState({});\n"
				   "state.noise_type = {};\n"
				   "state.fractal_type = {};\n"
				   "state.octaves = {};\n"
				   "state.gain = {};\n"
				   "state.frequency = {};\n"
				   "state.lacunarity = {};\n",
			fnl.get_seed(), fnl.get_noise_type(), fnl.get_fractal_type(), fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(), 1.0 / fnl.get_period(), fnl.get_fractal_lacunarity());
}

NodeTypeDB::NodeTypeDB() {
	typedef Runtime::ProcessBufferContext ProcessBufferContext;
	typedef Runtime::RangeAnalysisContext RangeAnalysisContext;

	FixedArray<NodeType, VoxelGraphFunction::NODE_TYPE_COUNT> &types = _types;

	// TODO Most operations need SIMD support

	// SUGG the program could be a list of pointers to polymorphic heap-allocated classes...
	// but I find that the data struct approach is kinda convenient too?

	{
		NodeType &t = types[VoxelGraphFunction::NODE_CONSTANT];
		t.name = "Constant";
		t.category = CATEGORY_CONSTANT;
		t.outputs.push_back(NodeType::Port("value"));
		t.params.push_back(NodeType::Param("value", Variant::FLOAT));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_X];
		t.name = "InputX";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("x"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_Y];
		t.name = "InputY";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("y"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_Z];
		t.name = "InputZ";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("z"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_INPUT_SDF];
		t.name = "InputSDF";
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("sdf"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_SDF];
		t.name = "OutputSDF";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("sdf", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			ZN_ASSERT(out.data != nullptr);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_WEIGHT];
		t.name = "OutputWeight";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("weight"));
		t.outputs.push_back(NodeType::Port("_out"));
		NodeType::Param layer_param("layer", Variant::INT, 0);
		layer_param.has_range = true;
		layer_param.min_value = 0;
		layer_param.max_value = 15;
		t.params.push_back(layer_param);
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = clamp(input.data[i], 0.f, 1.f);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, clamp(a, Interval::from_single_value(0.f), Interval::from_single_value(1.f)));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_TYPE];
		t.name = "OutputType";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("type"));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_OUTPUT_SINGLE_TEXTURE];
		t.name = "OutputSingleTexture";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("index"));
		t.outputs.push_back(NodeType::Port("_out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_CUSTOM_INPUT];
		t.name = "CustomInput";
		// t.params.push_back(NodeType::Param("binding", Variant::INT, 0));
		t.category = CATEGORY_INPUT;
		t.outputs.push_back(NodeType::Port("value"));
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_CUSTOM_OUTPUT];
		t.name = "CustomOutput";
		t.category = CATEGORY_OUTPUT;
		t.inputs.push_back(NodeType::Port("value"));
		t.outputs.push_back(NodeType::Port("_out"));
		// t.params.push_back(NodeType::Param("binding", Variant::INT, 0));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &input = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			memcpy(out.data, input.data, input.size * sizeof(float));
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			ctx.set_output(0, a);
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_RELAY];
		t.name = "Relay";
		t.category = CATEGORY_RELAY;
		t.inputs.push_back(NodeType::Port("in"));
		t.outputs.push_back(NodeType::Port("out"));
		t.is_pseudo_node = true;
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_ADD];
		t.name = "Add";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("a", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.inputs.push_back(NodeType::Port("b", 0.f, VoxelGraphFunction::AUTO_CONNECT_NONE, false));
		t.outputs.push_back(NodeType::Port("out"));
		t.compile_func = nullptr;
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a + b; });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a - b; });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a * b; });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, a / b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} / {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
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
		NodeType &t = types[VoxelGraphFunction::NODE_DISTANCE_2D];
		t.name = "Distance2D";
		t.category = CATEGORY_MATH;
		t.inputs.push_back(NodeType::Port("x0"));
		t.inputs.push_back(NodeType::Port("y0"));
		t.inputs.push_back(NodeType::Port("x1", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y1", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x0 = ctx.get_input(0);
			const Runtime::Buffer &y0 = ctx.get_input(1);
			const Runtime::Buffer &x1 = ctx.get_input(2);
			const Runtime::Buffer &y1 = ctx.get_input(3);
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = Math::sqrt(squared(x1.data[i] - x0.data[i]) + squared(y1.data[i] - y0.data[i]));
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
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
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
			// TODO Should be `const` but isn't because it auto-bakes, and it's a concern for multithreading
			Curve *curve;
			const CurveRangeData *curve_range_data;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_CURVE];
		t.name = "Curve";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("x"));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("curve", Curve::get_class_static(), []() {
			Ref<Curve> curve;
			curve.instantiate();
			// The default preset when creating a Curve isn't convenient.
			// Let's use a linear preset.
			curve->add_point(Vector2(0, 0));
			curve->add_point(Vector2(1, 1));
			curve->set_point_right_mode(0, Curve::TANGENT_LINEAR);
			curve->set_point_left_mode(1, Curve::TANGENT_LINEAR);
			return Variant(curve);
		}));
		t.compile_func = [](CompileContext &ctx) {
			Ref<Curve> curve = ctx.get_param(0);
			if (curve.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Curve::get_class_static())));
				return;
			}
			// Make sure it is baked. We don't want multithreading to bail out because of a write operation
			// happening in `interpolate_baked`...
			curve->bake();
			CurveRangeData *curve_range_data = memnew(CurveRangeData);
			get_curve_monotonic_sections(**curve, curve_range_data->sections);
			Params p;
			p.curve_range_data = curve_range_data;
			p.curve = *curve;
			ctx.set_params(p);
			ctx.add_memdelete_cleanup(curve_range_data);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_CURVE");
			const Runtime::Buffer &a = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.curve->sample_baked(a.data[i]);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Params p = ctx.get_params<Params>();
			if (a.is_single_value()) {
				const float v = p.curve->sample_baked(a.min);
				ctx.set_output(0, Interval::from_single_value(v));
			} else {
				const Interval r = get_curve_range(*p.curve, p.curve_range_data->sections, a);
				ctx.set_output(0, r);
			}
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<Curve> curve = ctx.get_param(0);
			if (curve.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Curve::get_class_static())));
				return;
			}
			ComputeShaderResource res;
			res.create_texture_2d(**curve);
			const std::string uniform_texture = ctx.add_uniform(std::move(res));
			ctx.add_format("{} = texture({}, vec2({}, 0.0)).r;\n", ctx.get_output_name(0), uniform_texture,
					ctx.get_input_name(0));
		};
	}
	{
		struct Params {
			// TODO Cannot be `const` because of an oversight in Godot, but the devs are not sure to do it
			// TODO We therefore have no guarantee it is thread-safe to use...
			Noise *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_NOISE_2D];
		t.name = "Noise2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(
				NodeType::Param("noise", Noise::get_class_static(), &create_resource_to_variant<FastNoiseLite>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<Noise> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Noise::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_NOISE_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_2d(x.data[i], y.data[i]);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_range_2d(*p.noise, x, y));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<Noise> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Noise::get_class_static())));
				return;
			}
			Ref<FastNoiseLite> fnl = noise;
			if (fnl.is_null()) {
				ctx.make_error(String(ZN_TTR("Shader generation with {0} is not supported."))
									   .format(varray(noise->get_class())));
				return;
			}
			ctx.require_lib_code("vg_fnl", zylann::fast_noise_lite::GDSHADER_SOURCE);
			add_fast_noise_lite_state_config(ctx, **fnl);
			ctx.add_format("{} = fnlGetNoise2D(state, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1));
		};
	}
	{
		struct Params {
			// TODO Cannot be `const` because of an oversight in Godot, but the devs are not sure to do it
			// TODO We therefore have no guarantee it is thread-safe to use...
			Noise *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_NOISE_3D];
		t.name = "Noise3D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(
				NodeType::Param("noise", Noise::get_class_static(), &create_resource_to_variant<FastNoiseLite>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<Noise> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Noise::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_NOISE_3D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_3d(x.data[i], y.data[i], z.data[i]);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_range_3d(*p.noise, x, y, z));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<Noise> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Noise::get_class_static())));
				return;
			}
			Ref<FastNoiseLite> fnl = noise;
			if (fnl.is_null()) {
				ctx.make_error(String(ZN_TTR("Shader generation with {0} is not supported."))
									   .format(varray(noise->get_class())));
				return;
			}
			ctx.require_lib_code("vg_fnl", zylann::fast_noise_lite::GDSHADER_SOURCE);
			add_fast_noise_lite_state_config(ctx, **fnl);
			// TODO Add missing options
			ctx.add_format("{} = fnlGetNoise3D(state, {}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2));
		};
	}
	{
		struct Params {
			const Image *image;
			const ImageRangeGrid *image_range_grid;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_IMAGE_2D];
		t.name = "Image";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("image", Image::get_class_static(), nullptr));
		t.compile_func = [](CompileContext &ctx) {
			Ref<Image> image = ctx.get_param(0);
			if (image.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Image::get_class_static())));
				return;
			}
			if (image->is_compressed()) {
				ctx.make_error(String(ZN_TTR("{0} has a compressed format, this is not supported"))
									   .format(varray(Image::get_class_static())));
				return;
			}
			ImageRangeGrid *im_range = memnew(ImageRangeGrid);
			im_range->generate(**image);
			Params p;
			p.image = *image;
			p.image_range_grid = im_range;
			ctx.set_params(p);
			ctx.add_memdelete_cleanup(im_range);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_IMAGE_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			// TODO Allow to use bilinear filtering?
			const Params p = ctx.get_params<Params>();
			const Image &im = *p.image;
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = get_pixel_repeat(im, x.data[i], y.data[i]);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			ctx.set_output(0, p.image_range_grid->get_range(x, y));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_PLANE];
		t.name = "SdfPlane";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("height"));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			do_binop(ctx, [](float a, float b) { return a - b; });
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			ctx.set_output(0, a - b);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = {} - {};\n", ctx.get_output_name(0), ctx.get_input_name(0), ctx.get_input_name(1));
		};
	}
	{
		struct Params {
			float size_x;
			float size_y;
			float size_z;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_BOX];
		t.name = "SdfBox";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.params.push_back(NodeType::Param("size_x", Variant::FLOAT, 10.0));
		t.params.push_back(NodeType::Param("size_y", Variant::FLOAT, 10.0));
		t.params.push_back(NodeType::Param("size_z", Variant::FLOAT, 10.0));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.size_x = ctx.get_param(0);
			p.size_y = ctx.get_param(1);
			p.size_z = ctx.get_param(2);
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			Runtime::Buffer &out = ctx.get_output(0);
			const Vector3 size(p.size_x, p.size_y, p.size_z);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = math::sdf_box(Vector3(x.data[i], y.data[i], z.data[i]), size);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			const Interval sx = Interval::from_single_value(p.size_x);
			const Interval sy = Interval::from_single_value(p.size_y);
			const Interval sz = Interval::from_single_value(p.size_z);
			ctx.set_output(0, math::sdf_box(x, y, z, sx, sy, sz));
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("sdf_box",
					"float vg_sdf_box(vec3 p, vec3 b) {\n"
					"	vec3 q = abs(p) - b;\n"
					"	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);\n"
					"}\n");
			ctx.add_format("{} = vg_sdf_box(vec3({}, {}, {}), vec3({}, {}, {}));\n", ctx.get_output_name(0),
					ctx.get_input_name(0), ctx.get_input_name(1), ctx.get_input_name(2), float(ctx.get_param(0)),
					float(ctx.get_param(1)), float(ctx.get_param(2)));
		};
	}
	{
		struct Params {
			float radius;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_SPHERE];
		t.name = "SdfSphere";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.params.push_back(NodeType::Param("radius", Variant::FLOAT, 1.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.radius = ctx.get_param(0);
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = Math::sqrt(squared(x.data[i]) + squared(y.data[i]) + squared(z.data[i])) - p.radius;
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			const Interval r = Interval::from_single_value(p.radius);
			ctx.set_output(0, get_length(x, y, z) - r);
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.add_format("{} = length(vec3({}, {}, {})) - {};\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2), float(ctx.get_param(0)));
		};
	}
	{
		struct Params {
			float r1;
			float r2;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_TORUS];
		t.name = "SdfTorus";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.params.push_back(NodeType::Param("radius1", Variant::FLOAT, 16.f));
		t.params.push_back(NodeType::Param("radius2", Variant::FLOAT, 4.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.r1 = ctx.get_param(0);
			p.r2 = ctx.get_param(1);
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = math::sdf_torus(x.data[i], y.data[i], z.data[i], p.r1, p.r2);
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			const Interval r1 = Interval::from_single_value(p.r1);
			const Interval r2 = Interval::from_single_value(p.r2);
			ctx.set_output(0, math::sdf_torus(x, y, z, r1, r2));
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("sdf_torus",
					"float vg_sdf_torus(vec3 p, vec2 t) {\n"
					"	vec2 q = vec2(length(p.xz) - t.x, p.y);\n"
					"	return length(q) - t.y;\n"
					"}\n");
			ctx.add_format("{} = vg_sdf_torus(vec3({}, {}, {}), vec2({}, {}));\n", ctx.get_output_name(0),
					ctx.get_input_name(0), ctx.get_input_name(1), ctx.get_input_name(2), float(ctx.get_param(0)),
					float(ctx.get_param(1)));
		};
	}
	{
		struct Params {
			float smoothness;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_SMOOTH_UNION];
		t.name = "SdfSmoothUnion";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("a"));
		t.inputs.push_back(NodeType::Port("b"));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.params.push_back(NodeType::Param("smoothness", Variant::FLOAT, 0.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.smoothness = ctx.get_param(0).operator float();
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_SDF_SMOOTH_UNION");
			bool a_ignored;
			bool b_ignored;
			const Runtime::Buffer &a = ctx.try_get_input(0, a_ignored);
			const Runtime::Buffer &b = ctx.try_get_input(1, b_ignored);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			if (a_ignored) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = b.data[i];
				}
			} else if (b_ignored) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = a.data[i];
				}
			} else if (params.smoothness > 0.0001f) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = math::sdf_smooth_union(a.data[i], b.data[i], params.smoothness);
				}
			} else {
				// Fallback on hard-union, smooth union does not support zero smoothness
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = math::sdf_union(a.data[i], b.data[i]);
				}
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			const Params params = ctx.get_params<Params>();

			if (params.smoothness > 0.0001f) {
				const math::SdfAffectingArguments args =
						math::sdf_polynomial_smooth_union_side(a, b, params.smoothness);
				switch (args) {
					case math::SDF_ONLY_A:
						ctx.ignore_input(1);
						break;
					case math::SDF_ONLY_B:
						ctx.ignore_input(0);
						break;
					case math::SDF_BOTH:
						break;
					default:
						CRASH_NOW();
						break;
				}
				ctx.set_output(0, math::sdf_smooth_union(a, b, params.smoothness));

			} else {
				const math::SdfAffectingArguments args = math::sdf_union_side(a, b);
				switch (args) {
					case math::SDF_ONLY_A:
						ctx.ignore_input(1);
						break;
					case math::SDF_ONLY_B:
						ctx.ignore_input(0);
						break;
					case math::SDF_BOTH:
						break;
					default:
						CRASH_NOW();
						break;
				}
				ctx.set_output(0, math::sdf_union(a, b));
			}
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("sdf_smooth_union",
					"float vg_sdf_smooth_union(float a, float b, float s) {\n"
					"	float h = clamp(0.5 + 0.5 * (b - a) / s, 0.0, 1.0);\n"
					"	return mix(b, a, h) - s * h * (1.0 - h);\n"
					"}\n");
			ctx.add_format("{} = vg_sdf_smooth_union({}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), float(ctx.get_param(0)));
		};
	}
	{
		struct Params {
			float smoothness;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_SMOOTH_SUBTRACT];
		t.name = "SdfSmoothSubtract";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("a"));
		t.inputs.push_back(NodeType::Port("b"));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.params.push_back(NodeType::Param("smoothness", Variant::FLOAT, 0.f));
		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.smoothness = ctx.get_param(0).operator float();
			ctx.set_params(p);
		};
		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_SDF_SMOOTH_SUBTRACT");
			bool a_ignored;
			bool b_ignored;
			const Runtime::Buffer &a = ctx.try_get_input(0, a_ignored);
			const Runtime::Buffer &b = ctx.try_get_input(1, b_ignored);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			if (a_ignored) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = -b.data[i];
				}
			} else if (b_ignored) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = a.data[i];
				}
			} else if (params.smoothness > 0.0001f) {
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = math::sdf_smooth_subtract(a.data[i], b.data[i], params.smoothness);
				}
			} else {
				// Fallback on hard-subtract, smooth subtract does not support zero smoothness
				for (uint32_t i = 0; i < out.size; ++i) {
					out.data[i] = math::sdf_subtract(a.data[i], b.data[i]);
				}
			}
		};
		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			const Params params = ctx.get_params<Params>();

			if (params.smoothness > 0.0001f) {
				const math::SdfAffectingArguments args =
						math::sdf_polynomial_smooth_subtract_side(a, b, params.smoothness);
				switch (args) {
					case math::SDF_ONLY_A:
						ctx.ignore_input(1);
						break;
					case math::SDF_ONLY_B:
						ctx.ignore_input(0);
						break;
					case math::SDF_BOTH:
						break;
					default:
						CRASH_NOW();
						break;
				}
				ctx.set_output(0, math::sdf_smooth_subtract(a, b, params.smoothness));

			} else {
				const math::SdfAffectingArguments args = math::sdf_subtract_side(a, b);
				switch (args) {
					case math::SDF_ONLY_A:
						ctx.ignore_input(1);
						break;
					case math::SDF_ONLY_B:
						ctx.ignore_input(0);
						break;
					case math::SDF_BOTH:
						break;
					default:
						CRASH_NOW();
						break;
				}
				ctx.set_output(0, math::sdf_subtract(a, b));
			}
		};
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			ctx.require_lib_code("sdf_smooth_subtract",
					"float vg_sdf_smooth_subtract(float b, float a, float s) {\n"
					"	float h = clamp(0.5 - 0.5 * (b + a) / s, 0.0, 1.0);\n"
					"	return mix(b, -a, h) + s * h * (1.0 - h);\n"
					"}\n");
			ctx.add_format("{} = vg_sdf_smooth_subtract({}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), float(ctx.get_param(0)));
		};
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_PREVIEW];
		t.name = "SdfPreview";
		t.category = CATEGORY_DEBUG;
		t.inputs.push_back(NodeType::Port("value"));

		t.params.push_back(NodeType::Param("min_value", Variant::FLOAT, -1.f));
		t.params.push_back(NodeType::Param("max_value", Variant::FLOAT, 1.f));
		t.params.push_back(NodeType::Param("fraction_period", Variant::FLOAT, 10.f));

		// Matches an enum in editor code `VoxelGraphEditorNodePreview`
		NodeType::Param mode_param("mode", Variant::INT, 0);
		mode_param.enum_items.push_back("Greyscale");
		mode_param.enum_items.push_back("SDF");
		t.params.push_back(mode_param);

		t.debug_only = true;
		t.is_pseudo_node = true;
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_COMMENT];
		t.name = "Comment";
		t.category = CATEGORY_DEBUG;
		NodeType::Param text_param("text", Variant::STRING, Variant(""));
		text_param.multiline = true;
		t.params.push_back(text_param);
		t.debug_only = true;
		t.is_pseudo_node = true;
	}
	{
		NodeType &t = types[VoxelGraphFunction::NODE_FUNCTION];
		t.name = "Function";
		t.category = CATEGORY_FUNCTIONS;

		NodeType::Param func_param("_function", VoxelGraphFunction::get_class_static(), nullptr);
		func_param.hidden = true;
		t.params.push_back(func_param);

		t.debug_only = false;
		t.is_pseudo_node = true;
	}
	{
		struct Params {
			float threshold;
		};
		NodeType &t = types[VoxelGraphFunction::NODE_SELECT];
		// t < threshold ? a : b
		t.name = "Select";
		t.category = CATEGORY_CONVERT;
		t.inputs.push_back(NodeType::Port("a"));
		t.inputs.push_back(NodeType::Port("b"));
		t.inputs.push_back(NodeType::Port("t"));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("threshold", Variant::FLOAT, 0.f));

		t.compile_func = [](CompileContext &ctx) {
			Params p;
			p.threshold = ctx.get_param(0).operator float();
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			bool a_ignored;
			bool b_ignored;
			const Runtime::Buffer &a = ctx.try_get_input(0, a_ignored);
			const Runtime::Buffer &b = ctx.try_get_input(1, b_ignored);
			const Runtime::Buffer &tested_value = ctx.get_input(2);
			const float threshold = ctx.get_params<Params>().threshold;

			Runtime::Buffer &out = ctx.get_output(0);

			const uint32_t buffer_size = out.size;

			if (a_ignored) {
				memcpy(out.data, b.data, buffer_size * sizeof(float));

			} else if (b_ignored) {
				memcpy(out.data, a.data, buffer_size * sizeof(float));

			} else if (tested_value.is_constant) {
				const float *src = tested_value.constant_value < threshold ? a.data : b.data;
				memcpy(out.data, src, buffer_size * sizeof(float));

			} else if (a.is_constant && b.is_constant && a.constant_value == b.constant_value) {
				memcpy(out.data, a.data, buffer_size * sizeof(float));

			} else {
				for (uint32_t i = 0; i < buffer_size; ++i) {
					out.data[i] = select(a.data[i], b.data[i], threshold, tested_value.data[i]);
				}
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval a = ctx.get_input(0);
			const Interval b = ctx.get_input(1);
			const Interval tested_value = ctx.get_input(2);
			const float threshold = ctx.get_params<Params>().threshold;

			if (tested_value.min >= threshold) {
				ctx.set_output(0, b);
				// `a` won't be used
				ctx.ignore_input(0);

			} else if (tested_value.max < threshold) {
				ctx.set_output(0, a);
				// `b` won't be used
				ctx.ignore_input(1);

			} else {
				ctx.set_output(0, Interval::from_union(a, b));
			}
		};
	}
	{
		struct Params {
			float radius;
			float factor;
			float min_height;
			float max_height;
			float norm_x;
			float norm_y;
			const Image *image;
			const ImageRangeGrid *image_range_grid;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_SDF_SPHERE_HEIGHTMAP];
		t.name = "SdfSphereHeightmap";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("sdf"));
		t.params.push_back(NodeType::Param("image", Image::get_class_static(), nullptr));
		t.params.push_back(NodeType::Param("radius", Variant::FLOAT, 10.f));
		t.params.push_back(NodeType::Param("factor", Variant::FLOAT, 1.f));

		t.compile_func = [](CompileContext &ctx) {
			Ref<Image> image = ctx.get_param(0);
			if (image.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(Image::get_class_static())));
				return;
			}
			if (image->is_compressed()) {
				ctx.make_error(String(ZN_TTR("{0} has a compressed format, this is not supported"))
									   .format(varray(Image::get_class_static())));
				return;
			}
			ImageRangeGrid *im_range = memnew(ImageRangeGrid);
			im_range->generate(**image);
			const float factor = ctx.get_param(2);
			const Interval range = im_range->get_range() * factor;
			Params p;
			p.min_height = range.min;
			p.max_height = range.max;
			p.image = *image;
			p.image_range_grid = im_range;
			p.radius = ctx.get_param(1);
			p.factor = factor;
			p.norm_x = image->get_width();
			p.norm_y = image->get_height();
			ctx.set_params(p);
			ctx.add_memdelete_cleanup(im_range);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_SDF_SPHERE_HEIGHTMAP");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			// TODO Allow to use bilinear filtering?
			const Params p = ctx.get_params<Params>();
			const Image &im = *p.image;
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = sdf_sphere_heightmap(x.data[i], y.data[i], z.data[i], p.radius, p.factor, im,
						p.min_height, p.max_height, p.norm_x, p.norm_y);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			ctx.set_output(
					0, sdf_sphere_heightmap(x, y, z, p.radius, p.factor, p.image_range_grid, p.norm_x, p.norm_y));
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

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
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

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
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
	{
		struct Params {
			const ZN_FastNoiseLite *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_2D];
		t.name = "FastNoise2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param(
				"noise", ZN_FastNoiseLite::get_class_static(), &create_resource_to_variant<ZN_FastNoiseLite>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_2d(x.data[i], y.data[i]);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_fnl_range_2d(*p.noise, x, y));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static())));
				return;
			}
			ctx.require_lib_code("vg_fnl", zylann::fast_noise_lite::GDSHADER_SOURCE);
			add_fast_noise_lite_state_config(ctx, **noise);
			ctx.add_format("{} = fnlGetNoise2D(state, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1));
		};
	}
	{
		struct Params {
			const ZN_FastNoiseLite *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_3D];
		t.name = "FastNoise3D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param(
				"noise", ZN_FastNoiseLite::get_class_static(), &create_resource_to_variant<ZN_FastNoiseLite>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_3D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_3d(x.data[i], y.data[i], z.data[i]);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_fnl_range_3d(*p.noise, x, y, z));
		};

		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static())));
				return;
			}
			ctx.require_lib_code("vg_fnl", zylann::fast_noise_lite::GDSHADER_SOURCE);
			add_fast_noise_lite_state_config(ctx, **noise);
			ctx.add_format("{} = fnlGetNoise3D(state, {}, {}, {});\n", ctx.get_output_name(0), ctx.get_input_name(0),
					ctx.get_input_name(1), ctx.get_input_name(2));
		};
	}
	{
		struct Params {
			ZN_FastNoiseLiteGradient *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_GRADIENT_2D];
		t.name = "FastNoiseGradient2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out_x"));
		t.outputs.push_back(NodeType::Port("out_y"));
		t.params.push_back(NodeType::Param("noise", ZN_FastNoiseLiteGradient::get_class_static(),
				&create_resource_to_variant<ZN_FastNoiseLiteGradient>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLiteGradient> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null"))
									   .format(varray(ZN_FastNoiseLiteGradient::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_GRADIENT_2D");
			const Runtime::Buffer &xb = ctx.get_input(0);
			const Runtime::Buffer &yb = ctx.get_input(1);
			Runtime::Buffer &out_x = ctx.get_output(0);
			Runtime::Buffer &out_y = ctx.get_output(1);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out_x.size; ++i) {
				real_t x = xb.data[i];
				real_t y = yb.data[i];
				p.noise->warp_2d(x, y);
				out_x.data[i] = x;
				out_y.data[i] = y;
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			const math::Interval2 r = get_fnl_gradient_range_2d(*p.noise, x, y);
			ctx.set_output(0, r.x);
			ctx.set_output(1, r.y);
		};
	}
	{
		struct Params {
			ZN_FastNoiseLiteGradient *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_GRADIENT_3D];
		t.name = "FastNoiseGradient3D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out_x"));
		t.outputs.push_back(NodeType::Port("out_y"));
		t.outputs.push_back(NodeType::Port("out_z"));
		t.params.push_back(NodeType::Param("noise", ZN_FastNoiseLiteGradient::get_class_static(),
				&create_resource_to_variant<ZN_FastNoiseLiteGradient>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLiteGradient> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null"))
									   .format(varray(ZN_FastNoiseLiteGradient::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_GRADIENT_3D");
			const Runtime::Buffer &xb = ctx.get_input(0);
			const Runtime::Buffer &yb = ctx.get_input(1);
			const Runtime::Buffer &zb = ctx.get_input(2);
			Runtime::Buffer &out_x = ctx.get_output(0);
			Runtime::Buffer &out_y = ctx.get_output(1);
			Runtime::Buffer &out_z = ctx.get_output(2);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out_x.size; ++i) {
				real_t x = xb.data[i];
				real_t y = yb.data[i];
				real_t z = zb.data[i];
				p.noise->warp_3d(x, y, z);
				out_x.data[i] = x;
				out_y.data[i] = y;
				out_z.data[i] = z;
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			const math::Interval3 r = get_fnl_gradient_range_3d(*p.noise, x, y, z);
			ctx.set_output(0, r.x);
			ctx.set_output(1, r.y);
			ctx.set_output(2, r.z);
		};
	}
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	{
		struct Params {
			const FastNoise2 *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_2_2D];
		t.name = "FastNoise2_2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(
				NodeType::Param("noise", FastNoise2::get_class_static(), &create_resource_to_variant<FastNoise2>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<FastNoise2> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(FastNoise2::get_class_static())));
				return;
			}
			noise->update_generator();
			if (!noise->is_valid()) {
				ctx.make_error(String(ZN_TTR("{0} setup is invalid")).format(varray(FastNoise2::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			p.noise->get_noise_2d_series(Span<const float>(x.data, x.size), Span<const float>(y.data, y.size),
					Span<float>(out.data, out.size));
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			// const Interval x = ctx.get_input(0);
			// const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			ZN_ASSERT_RETURN(p.noise != nullptr);
			ctx.set_output(0, p.noise->get_estimated_output_range());
		};
	}
	{
		struct Params {
			const FastNoise2 *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_2_3D];
		t.name = "FastNoise2_3D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(
				NodeType::Param("noise", FastNoise2::get_class_static(), &create_resource_to_variant<FastNoise2>));

		t.compile_func = [](CompileContext &ctx) {
			Ref<FastNoise2> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null")).format(varray(FastNoise2::get_class_static())));
				return;
			}
			noise->update_generator();
			if (!noise->is_valid()) {
				ctx.make_error(String(ZN_TTR("{0} setup is invalid")).format(varray(FastNoise2::get_class_static())));
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2_3D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			p.noise->get_noise_3d_series(Span<const float>(x.data, x.size), Span<const float>(y.data, y.size),
					Span<const float>(z.data, z.size), Span<float>(out.data, out.size));
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			// const Interval x = ctx.get_input(0);
			// const Interval y = ctx.get_input(1);
			// const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			ZN_ASSERT_RETURN(p.noise != nullptr);
			ctx.set_output(0, p.noise->get_estimated_output_range());
		};
	}
#endif // VOXEL_ENABLE_FAST_NOISE_2
	{
		NodeType &t = types[VoxelGraphFunction::NODE_EXPRESSION];
		t.name = "Expression";
		t.category = CATEGORY_MATH;
		NodeType::Param expression_param("expression", Variant::STRING, "0");
		expression_param.multiline = false;
		t.params.push_back(expression_param);
		t.outputs.push_back(NodeType::Port("out"));
		t.compile_func = [](CompileContext &ctx) {
			ctx.make_error(ZN_TTR("Internal error, expression wasn't expanded"));
		};
		t.is_pseudo_node = true;
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
	{
		struct Params {
			int32_t seed;
			float cell_size;
			float jitter;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_SPOTS_2D];
		t.name = "Spots2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.inputs.push_back(NodeType::Port("spot_radius", 5.f));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("seed", Variant::INT, 42));
		{
			NodeType::Param p("cell_size", Variant::FLOAT, 32.0);
			p.min_value = 0.01;
			p.max_value = 1000000.0;
			p.has_range = true;
			t.params.push_back(p);
		}
		{
			NodeType::Param p("jitter", Variant::FLOAT, 0.9);
			p.min_value = 0.f;
			p.max_value = 1.f;
			p.has_range = true;
			t.params.push_back(p);
		}

		t.compile_func = [](CompileContext &ctx) {
			Params params;
			params.seed = ctx.get_param(0).operator int();
			params.cell_size = ctx.get_param(1);
			params.jitter = ctx.get_param(2);

			if (params.jitter < 0.f) {
				ctx.make_error(ZN_TTR("Jitter cannot be negative"));
				return;
			}
			if (params.jitter > 1.f) {
				ctx.make_error(ZN_TTR("Jitter must be between 0 and 1"));
				return;
			}

			if (params.cell_size < 0) {
				ctx.make_error(ZN_TTR("Cell size cannot be negative"));
				return;
			}
			if (params.cell_size < 0.01) {
				// To avoid division by zero
				ctx.make_error(ZN_TTR("Cell size is too small"));
				return;
			}

			ctx.set_params(params);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &spot_size = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = SpotNoise::spot_noise_2d(Vector2f(x.data[i], y.data[i]), params.cell_size,
						spot_size.data[i], params.jitter, params.seed);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval spot_size = ctx.get_input(2);
			const Params params = ctx.get_params<Params>();
			ctx.set_output(0,
					SpotNoise::spot_noise_2d_range(
							Interval2{ x, y }, params.cell_size, spot_size, params.jitter, params.seed));
		};

		// TODO Support shader code for the Spots2D node
		// t.shader_gen_func = [](ShaderGenContext &ctx) {
		// };
	}
	{
		struct Params {
			int32_t seed;
			float cell_size;
			float jitter;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_SPOTS_3D];
		t.name = "Spots3D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("z", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.inputs.push_back(NodeType::Port("spot_radius", 5.f));
		t.outputs.push_back(NodeType::Port("out"));
		t.params.push_back(NodeType::Param("seed", Variant::INT, 42));
		{
			NodeType::Param p("cell_size", Variant::FLOAT, 32.0);
			p.min_value = 0.01;
			p.max_value = 1000000.0;
			p.has_range = true;
			t.params.push_back(p);
		}
		{
			NodeType::Param p("jitter", Variant::FLOAT, 0.9);
			p.min_value = 0.f;
			p.max_value = 1.f;
			p.has_range = true;
			t.params.push_back(p);
		}

		t.compile_func = [](CompileContext &ctx) {
			Params params;
			params.seed = ctx.get_param(0).operator int();
			params.cell_size = ctx.get_param(1);
			params.jitter = ctx.get_param(2);

			if (params.jitter < 0.f) {
				ctx.make_error(ZN_TTR("Jitter cannot be negative"));
				return;
			}
			if (params.jitter > 1.f) {
				ctx.make_error(ZN_TTR("Jitter must be between 0 and 1"));
				return;
			}

			if (params.cell_size < 0) {
				ctx.make_error(ZN_TTR("Cell size cannot be negative"));
				return;
			}
			if (params.cell_size < 0.01) {
				// To avoid division by zero
				ctx.make_error(ZN_TTR("Cell size is too small"));
				return;
			}

			ctx.set_params(params);
		};

		t.process_buffer_func = [](ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			const Runtime::Buffer &spot_size = ctx.get_input(3);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = SpotNoise::spot_noise_3d(Vector3f(x.data[i], y.data[i], z.data[i]), params.cell_size,
						spot_size.data[i], params.jitter, params.seed);
			}
		};

		t.range_analysis_func = [](RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Interval spot_size = ctx.get_input(3);
			const Params params = ctx.get_params<Params>();
			ctx.set_output(0,
					SpotNoise::spot_noise_3d_range(
							Interval3{ x, y, z }, params.cell_size, spot_size, params.jitter, params.seed));
		};

		// TODO Support shader code for the Spots3D node
		// t.shader_gen_func = [](ShaderGenContext &ctx) {
		// };
	}

	CRASH_COND(_expression_functions.size() > 0);

	for (unsigned int i = 0; i < _types.size(); ++i) {
		NodeType &t = _types[i];
		_type_name_to_id.insert({ t.name, (VoxelGraphFunction::NodeTypeID)i });

		for (uint32_t param_index = 0; param_index < t.params.size(); ++param_index) {
			NodeType::Param &p = t.params[param_index];
			t.param_name_to_index.insert({ p.name, param_index });
			p.index = param_index;

			switch (p.type) {
				case Variant::FLOAT:
					if (p.default_value.get_type() == Variant::NIL) {
						p.default_value = 0.f;
					}
					break;

				case Variant::INT:
					if (p.default_value.get_type() == Variant::NIL) {
						p.default_value = 0;
					}
					break;

				case Variant::OBJECT:
				case Variant::STRING:
					break;

				default:
					CRASH_NOW();
					break;
			}
		}

		for (uint32_t input_index = 0; input_index < t.inputs.size(); ++input_index) {
			const NodeType::Port &p = t.inputs[input_index];
			t.input_name_to_index.insert({ p.name, input_index });
		}

		if (t.expression_func != nullptr) {
			CRASH_COND(t.expression_func_name == nullptr);
			ExpressionParser::Function f;
			f.argument_count = t.inputs.size();
			f.name = t.expression_func_name;
			f.func = t.expression_func;
			f.id = i;
			_expression_functions.push_back(f);
		}
	}
}

Dictionary NodeTypeDB::get_type_info_dict(uint32_t id) const {
	const NodeType &type = _types[id];

	Dictionary type_dict;
	type_dict["name"] = type.name;

	Array inputs;
	inputs.resize(type.inputs.size());
	for (size_t i = 0; i < type.inputs.size(); ++i) {
		const NodeType::Port &input = type.inputs[i];
		Dictionary d;
		d["name"] = input.name;
		inputs[i] = d;
	}

	Array outputs;
	outputs.resize(type.outputs.size());
	for (size_t i = 0; i < type.outputs.size(); ++i) {
		const NodeType::Port &output = type.outputs[i];
		Dictionary d;
		d["name"] = output.name;
		outputs[i] = d;
	}

	Array params;
	params.resize(type.params.size());
	for (size_t i = 0; i < type.params.size(); ++i) {
		const NodeType::Param &p = type.params[i];
		Dictionary d;
		d["name"] = p.name;
		d["type"] = p.type;
		d["class_name"] = p.class_name;
		d["default_value"] = p.default_value;
		params[i] = d;
	}

	type_dict["inputs"] = inputs;
	type_dict["outputs"] = outputs;
	type_dict["params"] = params;

	return type_dict;
}

bool NodeTypeDB::try_get_type_id_from_name(const String &name, VoxelGraphFunction::NodeTypeID &out_type_id) const {
	auto it = _type_name_to_id.find(name);
	if (it == _type_name_to_id.end()) {
		return false;
	}
	out_type_id = it->second;
	return true;
}

bool NodeTypeDB::try_get_param_index_from_name(uint32_t type_id, const String &name, uint32_t &out_param_index) const {
	ERR_FAIL_INDEX_V(type_id, _types.size(), false);
	const NodeType &t = _types[type_id];
	auto it = t.param_name_to_index.find(name);
	if (it == t.param_name_to_index.end()) {
		return false;
	}
	out_param_index = it->second;
	return true;
}

bool NodeTypeDB::try_get_input_index_from_name(uint32_t type_id, const String &name, uint32_t &out_input_index) const {
	ERR_FAIL_INDEX_V(type_id, _types.size(), false);
	const NodeType &t = _types[type_id];
	auto it = t.input_name_to_index.find(name);
	if (it == t.input_name_to_index.end()) {
		return false;
	}
	out_input_index = it->second;
	return true;
}

VoxelGraphFunction::Port make_port_from_io_node(const ProgramGraph::Node &node, const NodeType &type) {
	ZN_ASSERT(type.category == CATEGORY_INPUT || type.category == CATEGORY_OUTPUT);

	VoxelGraphFunction::Port port;
	port.type = VoxelGraphFunction::NodeTypeID(node.type_id);

	switch (port.type) {
		case VoxelGraphFunction::NODE_CUSTOM_INPUT:
		case VoxelGraphFunction::NODE_CUSTOM_OUTPUT:
			port.name = node.name;
			break;

		case VoxelGraphFunction::NODE_OUTPUT_WEIGHT:
			ZN_ASSERT(node.params.size() >= 1);
			port.sub_index = node.params[0];
			ZN_ASSERT(type.outputs.size() == 1);
			port.name = type.outputs[0].name + String("_") + String::num_int64(port.sub_index);
			break;

		default:
			if (type.category == CATEGORY_INPUT) {
				ZN_ASSERT(type.outputs.size() == 1);
				port.name = type.outputs[0].name;
			} else {
				ZN_ASSERT(type.inputs.size() == 1);
				port.name = type.inputs[0].name;
			}
			break;
	}

	return port;
}

bool is_node_matching_port(const ProgramGraph::Node &node, const VoxelGraphFunction::Port &port) {
	if (node.type_id != port.type) {
		return false;
	}

	if (port.is_custom()) {
		return port.name == node.name;
	}

	if (node.type_id == VoxelGraphFunction::NODE_OUTPUT_WEIGHT) {
		ZN_ASSERT(node.params.size() >= 1);
		const unsigned int sub_index = node.params[0];
		if (sub_index != port.sub_index) {
			return false;
		}
	}

	return true;
}

} // namespace zylann::voxel::pg
