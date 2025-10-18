#include "../../../util/godot/classes/fast_noise_lite.h"
#include "../../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../../util/noise/fast_noise_lite/fast_noise_lite_range.h"
#include "../../../util/noise/gd_noise_range.h"
#include "../../../util/noise/spot_noise.h"
#include "../../../util/profiling.h"
#include "../node_type_db.h"

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../../../util/noise/fast_noise_2.h"
#endif

#ifdef VOXEL_ENABLE_GPU
#include "../../../shaders/fast_noise_lite_shader.h"
#endif

namespace zylann::voxel::pg {

template <typename T>
Variant create_resource_to_variant() {
	Ref<T> res;
	res.instantiate();
	return Variant(res);
}

#ifdef VOXEL_ENABLE_GPU

int godot_domain_warp_to_fnl(FastNoiseLite::DomainWarpFractalType gd_domain_warp_type) {
	switch (gd_domain_warp_type) {
		case FastNoiseLite::DOMAIN_WARP_FRACTAL_NONE:
			return ::fast_noise_lite::FastNoiseLite::FractalType_None;
		case FastNoiseLite::DOMAIN_WARP_FRACTAL_PROGRESSIVE:
			return ::fast_noise_lite::FastNoiseLite::FractalType_DomainWarpProgressive;
		case FastNoiseLite::DOMAIN_WARP_FRACTAL_INDEPENDENT:
			return ::fast_noise_lite::FastNoiseLite::FractalType_DomainWarpIndependent;
		default:
			return ::fast_noise_lite::FastNoiseLite::FractalType_None;
	}
}

void add_fast_noise_lite_state_config(ShaderGenContext &ctx, const FastNoiseLite &fnl) {
	// TODO Add missing options
	ctx.add_format(
			"fnl_state state = fnlCreateState({});\n"
			"state.noise_type = {};\n"
			"state.fractal_type = {};\n"
			"state.octaves = {};\n"
			"state.gain = {};\n"
			"state.frequency = {};\n"
			"state.lacunarity = {};\n"
			"state.cellular_distance_func = {};\n"
			"state.cellular_return_type = {};\n"
			"state.cellular_jitter_mod = {};\n",
			fnl.get_seed(),
			fnl.get_noise_type(),
			fnl.get_fractal_type(),
			fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(),
			fnl.get_frequency(),
			fnl.get_fractal_lacunarity(),
			fnl.get_cellular_distance_function(),
			fnl.get_cellular_return_type(),
			fnl.get_cellular_jitter()
	);
	if (fnl.is_domain_warp_enabled()) {
		ctx.add_format(
				"fnl_state warp_state = fnlCreateState({});\n"
				"warp_state.domain_warp_type = {};\n"
				"warp_state.domain_warp_amp = {};\n"
				"warp_state.fractal_type = {};\n"
				"warp_state.octaves = {};\n"
				"warp_state.gain = {};\n"
				"warp_state.frequency = {};\n"
				"warp_state.lacunarity = {};\n",
				fnl.get_seed(),
				fnl.get_noise_type(),
				fnl.get_domain_warp_amplitude(),
				godot_domain_warp_to_fnl(fnl.get_domain_warp_fractal_type()),
				fnl.get_fractal_octaves(),
				fnl.get_fractal_gain(),
				fnl.get_domain_warp_frequency(),
				fnl.get_domain_warp_fractal_lacunarity()
		);
	}
}

void add_fast_noise_lite_gradient_state_config(ShaderGenContext &ctx, const ZN_FastNoiseLiteGradient &fnl) {
	ctx.add_format(
			"fnl_state warp_state = fnlCreateState({});\n"
			"warp_state.domain_warp_type = {};\n"
			"warp_state.domain_warp_amp = {};\n"
			"warp_state.fractal_type = {};\n"
			"warp_state.octaves = {};\n"
			"warp_state.gain = {};\n"
			"warp_state.frequency = {};\n"
			"warp_state.lacunarity = {};\n",
			fnl.get_seed(),
			fnl.get_noise_type(),
			fnl.get_amplitude(),
			fnl.get_fractal_type_fnl(),
			fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(),
			1.0 / fnl.get_period(),
			fnl.get_fractal_lacunarity()
	);
}

void add_fast_noise_lite_state_config(ShaderGenContext &ctx, const ZN_FastNoiseLite &fnl) {
	// TODO Add missing options
	ctx.add_format(
			"fnl_state state = fnlCreateState({});\n"
			"state.noise_type = {};\n"
			"state.fractal_type = {};\n"
			"state.octaves = {};\n"
			"state.gain = {};\n"
			"state.frequency = {};\n"
			"state.lacunarity = {};\n"
			"state.cellular_distance_func = {};\n"
			"state.cellular_return_type = {};\n"
			"state.cellular_jitter_mod = {};\n",
			fnl.get_seed(),
			fnl.get_noise_type(),
			fnl.get_fractal_type(),
			fnl.get_fractal_octaves(),
			fnl.get_fractal_gain(),
			1.0 / fnl.get_period(),
			fnl.get_fractal_lacunarity(),
			fnl.get_cellular_distance_function(),
			fnl.get_cellular_return_type(),
			fnl.get_cellular_jitter()
	);
	Ref<ZN_FastNoiseLiteGradient> fnlg = fnl.get_warp_noise();
	if (fnlg.is_valid()) {
		add_fast_noise_lite_gradient_state_config(ctx, **fnlg);
	}
}

#endif

void register_noise_nodes(Span<NodeType> types) {
	using namespace math;

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
				NodeType::Param("noise", Noise::get_class_static(), &create_resource_to_variant<FastNoiseLite>)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_NOISE_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_2d(x.data[i], y.data[i]);
			}
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_range_2d(*p.noise, x, y));
		};

#ifdef VOXEL_ENABLE_GPU
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
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_state_config(ctx, **fnl);
			if (fnl->is_domain_warp_enabled()) {
				ctx.add_format(
						"float wx = {};\n"
						"float wy = {};\n"
						"fnlDomainWarp2D(warp_state, wx, wy);\n",
						"{} = fnlGetNoise2D(state, wx, wy);\n",
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_output_name(0)
				);
			} else {
				ctx.add_format(
						"{} = fnlGetNoise2D(state, {}, {});\n",
						ctx.get_output_name(0),
						ctx.get_input_name(0),
						ctx.get_input_name(1)
				);
			}
		};
#endif
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
				NodeType::Param("noise", Noise::get_class_static(), &create_resource_to_variant<FastNoiseLite>)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_range_3d(*p.noise, x, y, z));
		};

#ifdef VOXEL_ENABLE_GPU
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
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_state_config(ctx, **fnl);
			if (fnl->is_domain_warp_enabled()) {
				ctx.add_format(
						"float wx = {};\n"
						"float wy = {};\n"
						"float wz = {};\n"
						"fnlDomainWarp3D(warp_state, wx, wy, wz);\n"
						"{} = fnlGetNoise3D(state, wx, wy, wz);\n",
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_input_name(2),
						ctx.get_output_name(0)
				);
			} else {
				ctx.add_format(
						"{} = fnlGetNoise3D(state, {}, {}, {});\n",
						ctx.get_output_name(0),
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_input_name(2)
				);
			}
		};
#endif
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
		t.params.push_back(
				NodeType::Param(
						"noise", ZN_FastNoiseLite::get_class_static(), &create_resource_to_variant<ZN_FastNoiseLite>
				)
		);

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static()))
				);
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			for (uint32_t i = 0; i < out.size; ++i) {
				out.data[i] = p.noise->get_noise_2d(x.data[i], y.data[i]);
			}
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_fnl_range_2d(*p.noise, x, y));
		};

#ifdef VOXEL_ENABLE_GPU
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static()))
				);
				return;
			}
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_state_config(ctx, **noise);

			if (noise->get_warp_noise().is_valid()) {
				ctx.add_format(
						"float wx = {};\n"
						"float wy = {};\n"
						"fnlDomainWarp2D(warp_state, wx, wy);\n",
						"{} = fnlGetNoise2D(state, wx, wy);\n",
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_output_name(0)
				);
			} else {
				ctx.add_format(
						"{} = fnlGetNoise2D(state, {}, {});\n",
						ctx.get_output_name(0),
						ctx.get_input_name(0),
						ctx.get_input_name(1)
				);
			}
		};
#endif
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
		t.params.push_back(
				NodeType::Param(
						"noise", ZN_FastNoiseLite::get_class_static(), &create_resource_to_variant<ZN_FastNoiseLite>
				)
		);

		t.compile_func = [](CompileContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static()))
				);
				return;
			}
			Params p;
			p.noise = *noise;
			ctx.set_params(p);
		};

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			ctx.set_output(0, get_fnl_range_3d(*p.noise, x, y, z));
		};

#ifdef VOXEL_ENABLE_GPU
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLite> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(
						String(ZN_TTR("{0} instance is null")).format(varray(ZN_FastNoiseLite::get_class_static()))
				);
				return;
			}
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_state_config(ctx, **noise);

			if (noise->get_warp_noise().is_valid()) {
				ctx.add_format(
						"float wx = {};\n"
						"float wy = {};\n"
						"float wz = {};\n"
						"fnlDomainWarp3D(warp_state, wx, wy, wz);\n"
						"{} = fnlGetNoise3D(state, wx, wy, wz);\n",
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_input_name(2),
						ctx.get_output_name(0)
				);
			} else {
				ctx.add_format(
						"{} = fnlGetNoise3D(state, {}, {}, {});\n",
						ctx.get_output_name(0),
						ctx.get_input_name(0),
						ctx.get_input_name(1),
						ctx.get_input_name(2)
				);
			}
		};
#endif
	}
	{
		struct Params {
			const ZN_FastNoiseLiteGradient *noise;
		};

		NodeType &t = types[VoxelGraphFunction::NODE_FAST_NOISE_GRADIENT_2D];
		t.name = "FastNoiseGradient2D";
		t.category = CATEGORY_GENERATE;
		t.inputs.push_back(NodeType::Port("x", 0.f, VoxelGraphFunction::AUTO_CONNECT_X));
		t.inputs.push_back(NodeType::Port("y", 0.f, VoxelGraphFunction::AUTO_CONNECT_Z));
		t.outputs.push_back(NodeType::Port("out_x"));
		t.outputs.push_back(NodeType::Port("out_y"));
		t.params.push_back(
				NodeType::Param(
						"noise",
						ZN_FastNoiseLiteGradient::get_class_static(),
						&create_resource_to_variant<ZN_FastNoiseLiteGradient>
				)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Params p = ctx.get_params<Params>();
			// Shouldn't be null, it is checked when the graph is compiled
			const math::Interval2 r = get_fnl_gradient_range_2d(*p.noise, x, y);
			ctx.set_output(0, r.x);
			ctx.set_output(1, r.y);
		};

#ifdef VOXEL_ENABLE_GPU
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLiteGradient> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null"))
									   .format(varray(ZN_FastNoiseLiteGradient::get_class_static())));
				return;
			}
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_gradient_state_config(ctx, **noise);
			ctx.add_format(
					"float wx = {};\n"
					"float wy = {};\n"
					"fnlDomainWarp2D(warp_state, wx, wy);\n"
					"{} = wx;\n"
					"{} = wy;\n",
					ctx.get_input_name(0),
					ctx.get_input_name(1),
					ctx.get_output_name(0),
					ctx.get_output_name(1)
			);
		};
#endif
	}
	{
		struct Params {
			const ZN_FastNoiseLiteGradient *noise;
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
		t.params.push_back(
				NodeType::Param(
						"noise",
						ZN_FastNoiseLiteGradient::get_class_static(),
						&create_resource_to_variant<ZN_FastNoiseLiteGradient>
				)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
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

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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

#ifdef VOXEL_ENABLE_GPU
		t.shader_gen_func = [](ShaderGenContext &ctx) {
			Ref<ZN_FastNoiseLiteGradient> noise = ctx.get_param(0);
			if (noise.is_null()) {
				ctx.make_error(String(ZN_TTR("{0} instance is null"))
									   .format(varray(ZN_FastNoiseLiteGradient::get_class_static())));
				return;
			}
			ctx.require_lib_code("vg_fnl", g_fast_noise_lite_shader);
			add_fast_noise_lite_gradient_state_config(ctx, **noise);
			ctx.add_format(
					"float wx = {};\n"
					"float wy = {};\n"
					"float wz = {};\n"
					"fnlDomainWarp3D(warp_state, wx, wy, wz);\n"
					"{} = wx;\n"
					"{} = wy;\n"
					"{} = wz;\n",
					ctx.get_input_name(0),
					ctx.get_input_name(1),
					ctx.get_input_name(2),
					ctx.get_output_name(0),
					ctx.get_output_name(1),
					ctx.get_output_name(2)
			);
		};
#endif
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
				NodeType::Param("noise", FastNoise2::get_class_static(), &create_resource_to_variant<FastNoise2>)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2_2D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			p.noise->get_noise_2d_series(
					Span<const float>(x.data, x.size),
					Span<const float>(y.data, y.size),
					Span<float>(out.data, out.size)
			);
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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
				NodeType::Param("noise", FastNoise2::get_class_static(), &create_resource_to_variant<FastNoise2>)
		);

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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			ZN_PROFILE_SCOPE_NAMED("NODE_FAST_NOISE_2_3D");
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params p = ctx.get_params<Params>();
			p.noise->get_noise_3d_series(
					Span<const float>(x.data, x.size),
					Span<const float>(y.data, y.size),
					Span<const float>(z.data, z.size),
					Span<float>(out.data, out.size)
			);
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &spot_size = ctx.get_input(2);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = SpotNoise::spot_noise_2d(
						Vector2f(x.data[i], y.data[i]), params.cell_size, spot_size.data[i], params.jitter, params.seed
				);
			}
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval spot_size = ctx.get_input(2);
			const Params params = ctx.get_params<Params>();
			ctx.set_output(
					0,
					SpotNoise::spot_noise_2d_range(
							Interval2{ x, y }, params.cell_size, spot_size, params.jitter, params.seed
					)
			);
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

		t.process_buffer_func = [](Runtime::ProcessBufferContext &ctx) {
			const Runtime::Buffer &x = ctx.get_input(0);
			const Runtime::Buffer &y = ctx.get_input(1);
			const Runtime::Buffer &z = ctx.get_input(2);
			const Runtime::Buffer &spot_size = ctx.get_input(3);
			Runtime::Buffer &out = ctx.get_output(0);
			const Params params = ctx.get_params<Params>();
			for (unsigned int i = 0; i < out.size; ++i) {
				out.data[i] = SpotNoise::spot_noise_3d(
						Vector3f(x.data[i], y.data[i], z.data[i]),
						params.cell_size,
						spot_size.data[i],
						params.jitter,
						params.seed
				);
			}
		};

		t.range_analysis_func = [](Runtime::RangeAnalysisContext &ctx) {
			const Interval x = ctx.get_input(0);
			const Interval y = ctx.get_input(1);
			const Interval z = ctx.get_input(2);
			const Interval spot_size = ctx.get_input(3);
			const Params params = ctx.get_params<Params>();
			ctx.set_output(
					0,
					SpotNoise::spot_noise_3d_range(
							Interval3{ x, y, z }, params.cell_size, spot_size, params.jitter, params.seed
					)
			);
		};

		// TODO Support shader code for the Spots3D node
		// t.shader_gen_func = [](ShaderGenContext &ctx) {
		// };
	}
}

} // namespace zylann::voxel::pg
