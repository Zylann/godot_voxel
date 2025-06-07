#include "voxel_graph_editor_node_preview.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/shader.h"
#include "../../util/godot/classes/texture_rect.h"
#include "graph_editor_adapter.h"

namespace zylann::voxel {

namespace {

const char *g_sdf_greyscale_shader_code = //
		"shader_type canvas_item;\n"
		"uniform vec2 u_remap = vec2(1.0, 0.0);\n"
		"void fragment() {\n"
		"    float sd = texture(TEXTURE, UV).r;\n"
		"    sd = u_remap.x * sd + u_remap.y;\n"
		"    float g = clamp(sd, 0.0, 1.0);\n"
		"    COLOR = vec4(g, g, g, 1.0);\n"
		"}\n";

const char *g_sdf_color_shader_code = //
		"shader_type canvas_item;\n"
		"\n"
		"//uniform vec2 u_remap = vec2(1.0, 0.0);\n"
		"uniform float u_fraction_amount = 0.5;\n"
		"uniform float u_fraction_frequency = 0.1;\n"
		"uniform vec4 u_negative_color = vec4(0.5, 0.5, 1.0, 1.0);\n"
		"uniform vec4 u_positive_color = vec4(1.0, 0.7, 0.0, 1.0);\n"
		"\n"
		"void fragment() {\n"
		"    float sd = texture(TEXTURE, UV).r;\n"
		"    //sd *= u_remap.x * sd + u_remap.y;\n"
		"    float positive_amount = \n"
		"        mix(clamp(sd, 0.0, 1.0), fract(max(sd * u_fraction_frequency, 0.0)), u_fraction_amount);\n"
		"    float negative_amount = \n"
		"        mix(clamp(-sd, 0.0, 1.0), fract(max(-sd * u_fraction_frequency, 0.0)), u_fraction_amount);\n"
		"    vec3 col = u_negative_color.rgb * negative_amount + u_positive_color.rgb * positive_amount;\n"
		"    col *= 1.0 - u_fraction_amount * (1.0 - (fract(sign(sd) * sd * u_fraction_frequency)));\n"
		"    COLOR = vec4(col, 1.0);\n"
		"}\n";

FixedArray<Ref<Shader>, GraphEditorPreview::VIEW_MODE_COUNT> g_mode_shaders;

Ref<Shader> load_shader(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	return shader;
}

} // namespace

void VoxelGraphEditorNodePreview::load_resources() {
	g_mode_shaders[PIXEL_MODE_GREYSCALE] = load_shader(g_sdf_greyscale_shader_code);
	g_mode_shaders[PIXEL_MODE_SDF] = load_shader(g_sdf_color_shader_code);
}

void VoxelGraphEditorNodePreview::unload_resources() {
	for (unsigned int i = 0; i < g_mode_shaders.size(); ++i) {
		g_mode_shaders[i].unref();
	}
}

VoxelGraphEditorNodePreview::VoxelGraphEditorNodePreview() {
	_image = zylann::godot::create_empty_image(RESOLUTION, RESOLUTION, false, Image::FORMAT_RF);
	_image->fill(Color(0.5, 0.5, 0.5));
	_texture = ImageTexture::create_from_image(_image);
	_texture->update(_image);
	_texture_rect = memnew(TextureRect);
	_texture_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
	_texture_rect->set_custom_minimum_size(Vector2(RESOLUTION, RESOLUTION));
	_texture_rect->set_texture(_texture);
	_texture_rect->set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST);
	_material.instantiate();
	_texture_rect->set_material(_material);
	add_child(_texture_rect);
}

void VoxelGraphEditorNodePreview::update_from_buffer(const pg::Runtime::Buffer &buffer) {
	ERR_FAIL_COND(RESOLUTION * RESOLUTION != static_cast<int>(buffer.size));

	// TODO Support debugging inputs
	ERR_FAIL_COND_MSG(
			buffer.data == nullptr,
			buffer.is_binding ? "Plugging a debug view on an input is not supported yet."
							  : "Didn't expect buffer to be null"
	);

	PackedByteArray image_data;
	image_data.resize(buffer.size * sizeof(float));
	{
		// Not using `set_pixel` because it is a lot slower, especially through GDExtension
		float *image_data_w = reinterpret_cast<float *>(image_data.ptrw());
		for (unsigned int i = 0; i < buffer.size; ++i) {
			image_data_w[i] = buffer.data[i];
		}
	}

	_image = Image::create_from_data(RESOLUTION, RESOLUTION, false, Image::FORMAT_RF, image_data);
	_image->flip_y();

	_texture->update(_image);
}

void VoxelGraphEditorNodePreview::update_display_settings(const pg::VoxelGraphFunction &graph, uint32_t node_id) {
	const float min_value = graph.get_node_param(node_id, 0);
	const float max_value = graph.get_node_param(node_id, 1);
	const float fraction_period = graph.get_node_param(node_id, 2);
	const int mode = graph.get_node_param(node_id, 3);

	// Note, remap is only used with greyscale display mode, such that min is black and max is white.
	// When using SDF it's more useful to keep -1..1 to measure distortions of the field.
	const math::LinearFuncParams remap = math::remap_intervals_to_linear_params(min_value, max_value, 0.f, 1.f);

	ERR_FAIL_COND(_material.is_null());
	_material->set_shader(g_mode_shaders[mode]);
	_material->set_shader_parameter("u_remap", Vector2(remap.a, remap.b));
	_material->set_shader_parameter("u_fraction_amount", 0.5);
	_material->set_shader_parameter("u_fraction_frequency", fraction_period > 0.0001f ? 1.f / fraction_period : 0.f);
	_material->set_shader_parameter("u_negative_color", Color(0.5, 0.5, 1.0, 1.0));
	_material->set_shader_parameter("u_positive_color", Color(1.0, 0.7, 0.0, 1.0));
}

void VoxelGraphEditorNodePreview::update_previews(
		GraphEditorAdapter &adapter,
		Span<const PreviewInfo> previews,
		const GraphEditorPreview::ViewMode view_mode,
		const float transform_scale,
		const Vector2f transform_offset
) {
	// TODO Use a thread?
	ZN_PRINT_VERBOSE("Updating slice previews");

	if (previews.size() == 0) {
		return;
	}

	// Generate data
	{
		const int preview_size_x = RESOLUTION;
		const int preview_size_y = RESOLUTION;
		// TODO This might be too big for buffer size?
		const int buffer_size = preview_size_x * preview_size_y;
		StdVector<float> x_vec;
		StdVector<float> y_vec;
		StdVector<float> z_vec;
		x_vec.resize(buffer_size);
		y_vec.resize(buffer_size);
		z_vec.resize(buffer_size);

		const float view_size_x = transform_scale * float(preview_size_x);
		const float view_size_y = transform_scale * float(preview_size_x);
		const Vector3f min_pos =
				Vector3f(-view_size_x * 0.5f + transform_offset.x, -view_size_y * 0.5f + transform_offset.y, 0);
		const Vector3f max_pos = min_pos + Vector3f(view_size_x, view_size_y, 0);

		int i = 0;
		for (int iy = 0; iy < preview_size_x; ++iy) {
			const float y = Math::lerp(min_pos.y, max_pos.y, static_cast<float>(iy) / preview_size_y);
			for (int ix = 0; ix < preview_size_y; ++ix) {
				const float x = Math::lerp(min_pos.x, max_pos.x, static_cast<float>(ix) / preview_size_x);
				x_vec[i] = x;
				y_vec[i] = y;
				z_vec[i] = min_pos.z;
				++i;
			}
		}

		Span<float> x_coords = to_span(x_vec);
		Span<float> y_coords;
		Span<float> z_coords;
		if (view_mode == GraphEditorPreview::VIEW_SLICE_XY) {
			y_coords = to_span(y_vec);
			z_coords = to_span(z_vec);
		} else {
			y_coords = to_span(z_vec);
			z_coords = to_span(y_vec);
		}

		adapter.generate_set(x_coords, y_coords, z_coords);
	}

	const pg::Runtime::State &last_state = adapter.get_last_state_from_current_thread();

	// Update previews
	for (const PreviewInfo &info : previews) {
		const pg::Runtime::Buffer &buffer = last_state.get_buffer(info.address);
		info.control->update_from_buffer(buffer);
		info.control->update_display_settings(**adapter.graph, info.node_id);
	}
}

} // namespace zylann::voxel
