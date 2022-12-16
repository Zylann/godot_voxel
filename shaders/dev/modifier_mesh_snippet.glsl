#[compute]
#version 450

// <SNIPPET>

layout (set = 0, binding = 5) restrict readonly buffer ShapeParams {
	vec3 model_to_buffer_translation;
	vec3 model_to_buffer_scale;
	float isolevel;
} u_shape_params;

layout (set = 0, binding = 6) uniform sampler3D u_sd_buffer;

float get_sd(vec3 pos) {
	pos = u_shape_params.model_to_buffer_scale * (u_shape_params.model_to_buffer_translation + pos);
	return texture(u_sd_buffer, pos).r - u_shape_params.isolevel;
}

// </SNIPPET>

void main() {}
