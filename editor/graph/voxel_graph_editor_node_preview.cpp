#include "voxel_graph_editor_node_preview.h"
#include "../../generators/graph/voxel_graph_function.h"
#include "../../util/godot/classes/shader.h"
#include "../../util/godot/classes/texture_rect.h"

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

FixedArray<Ref<Shader>, VoxelGraphEditorNodePreview::MODE_COUNT> g_mode_shaders;

Ref<Shader> load_shader(const char *p_code) {
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(p_code);
	return shader;
}

} // namespace

void VoxelGraphEditorNodePreview::load_resources() {
	g_mode_shaders[MODE_GREYSCALE] = load_shader(g_sdf_greyscale_shader_code);
	g_mode_shaders[MODE_SDF] = load_shader(g_sdf_color_shader_code);
}

void VoxelGraphEditorNodePreview::unload_resources() {
	for (unsigned int i = 0; i < g_mode_shaders.size(); ++i) {
		g_mode_shaders[i].unref();
	}
}

VoxelGraphEditorNodePreview::VoxelGraphEditorNodePreview() {
	_image = create_empty_image(RESOLUTION, RESOLUTION, false, Image::FORMAT_RF);
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
	ERR_FAIL_COND_MSG(buffer.data == nullptr,
			buffer.is_binding ? "Plugging a debug view on an input is not supported yet."
							  : "Didn't expect buffer to be null");

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
	float remap_a;
	float remap_b;
	math::remap_intervals_to_linear_params(min_value, max_value, 0.f, 1.f, remap_a, remap_b);

	ERR_FAIL_COND(_material.is_null());
	_material->set_shader(g_mode_shaders[mode]);
	_material->set_shader_parameter("u_remap", Vector2(remap_a, remap_b));
	_material->set_shader_parameter("u_fraction_amount", 0.5);
	_material->set_shader_parameter("u_fraction_frequency", fraction_period > 0.0001f ? 1.f / fraction_period : 0.f);
	_material->set_shader_parameter("u_negative_color", Color(0.5, 0.5, 1.0, 1.0));
	_material->set_shader_parameter("u_positive_color", Color(1.0, 0.7, 0.0, 1.0));
}

} // namespace zylann::voxel
