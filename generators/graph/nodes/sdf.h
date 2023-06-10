#include "../../../util/math/sdf.h"
#include "../../../util/profiling.h"
#include "../node_type_db.h"
#include "util.h"

namespace zylann::voxel::pg {

void register_sdf_nodes(Span<NodeType> types) {
	using namespace math;

	{
		NodeType &t = types[VoxelGraphFunction::NODE_SDF_PLANE];
		t.name = "SdfPlane";
		t.category = CATEGORY_SDF;
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Y));
		t.inputs.push_back(NodeType::Port("height"));
		t.outputs.push_back(NodeType::Port("sdf"));
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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = Math::sqrt(squared(x.data[i]) + squared(y.data[i]) + squared(z.data[i])) - p.radius;
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			Runtime::Buffer &out = ctx.get_output(0);
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = math::sdf_torus(x.data[i], y.data[i], z.data[i], p.r1, p.r2);
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
}

} // namespace zylann::voxel::pg
