#include "../../../util/godot/classes/curve.h"
#include "../../../util/profiling.h"
#include "../node_type_db.h"
#include "../range_utility.h"

namespace zylann::voxel::pg {

void register_curve_node(Span<NodeType> types) {
	using namespace math;

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
		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_CURVE");
			const Runtime::Buffer &a = ctx.get_input(0);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.curve->sample_baked(a.data[i]);
			}
		};
		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
			// We are offsetting X to match the interpolation Godot's Curve does, because the default linear
			// interpolation sampler is offset by half a pixel
			ctx.add_format("{} = texture({}, vec2({} + 0.5 / float(textureSize({}, 0).x), 0.0)).r;\n",
					ctx.get_output_name(0), uniform_texture, ctx.get_input_name(0), uniform_texture);
		};
	}
}

} // namespace zylann::voxel::pg
