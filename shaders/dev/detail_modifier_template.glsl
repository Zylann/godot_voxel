#[compute]
#version 450

// Takes a list of positions and evaluates a signed distance field in 4 locations around them.
// The 4 locations are picked such that the result can be used to compute a gradient.
// The result is then applied on top of previous values using a specified operation.

// This shader may be dispatched multiple times for each source of voxel data that we may combine for a given chunk.

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict readonly buffer PositionBuffer {
	// X, Y, Z is hit position
	// W is integer triangle index
	vec4 values[];
} u_positions;

layout (set = 0, binding = 1, std430) restrict readonly buffer DetailParams {
	int tile_size_pixels;
	float pixel_world_step;
} u_detail_params;

layout (set = 0, binding = 2, std430) restrict readonly buffer InSDBuffer {
	// 4 values per index
	float values[];
} u_in_sd;

layout (set = 0, binding = 3, std430) restrict writeonly buffer OutSDBuffer {
	// 4 values per index
	float values[];
} u_out_sd;

// Parameters common to all modifiers
layout (set = 0, binding = 4, std430) restrict readonly buffer BaseModifierParams {
	mat4 world_to_model;
	int operation;
	float smoothness;
	float sd_scale;
} u_base_modifier_params;

// <PLACEHOLDER_SECTION>
float get_sd(vec3 pos) {
	return 0.0;
}
// </PLACEHOLDER_SECTION>

float sd_smooth_union(float a, float b, float s) {
	const float h = clamp(0.5 + 0.5 * (b - a) / s, 0.0, 1.0);
	return mix(b, a, h) - s * h * (1.0 - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
float sd_smooth_subtract(float b, float a, float s) {
	const float h = clamp(0.5 - 0.5 * (b + a) / s, 0.0, 1.0);
	return mix(b, -a, h) + s * h * (1.0 - h);
}

void main() {
	const ivec2 pixel_pos_in_tile = ivec2(gl_GlobalInvocationID.xy);
	const int tile_index = int(gl_GlobalInvocationID.z);

	const int index = pixel_pos_in_tile.x + pixel_pos_in_tile.y * u_detail_params.tile_size_pixels 
		+ tile_index * u_detail_params.tile_size_pixels * u_detail_params.tile_size_pixels;

	if (u_positions.values[index].w == -1.0) {
		return;
	}

	vec3 pos0 = u_positions.values[index].xyz;
	vec3 pos1 = pos0 + vec3(u_detail_params.pixel_world_step, 0.0, 0.0);
	vec3 pos2 = pos0 + vec3(0.0, u_detail_params.pixel_world_step, 0.0);
	vec3 pos3 = pos0 + vec3(0.0, 0.0, u_detail_params.pixel_world_step);

	pos0 = (u_base_modifier_params.world_to_model * vec4(pos0, 1.0)).xyz;
	pos1 = (u_base_modifier_params.world_to_model * vec4(pos1, 1.0)).xyz;
	pos2 = (u_base_modifier_params.world_to_model * vec4(pos2, 1.0)).xyz;
	pos3 = (u_base_modifier_params.world_to_model * vec4(pos3, 1.0)).xyz;

	const int sdi = index * 4;

	float sd0 = u_in_sd.values[sdi];
	float sd1 = u_in_sd.values[sdi + 1];
	float sd2 = u_in_sd.values[sdi + 2];
	float sd3 = u_in_sd.values[sdi + 3];

	const float msd0 = get_sd(pos0) * u_base_modifier_params.sd_scale;
	const float msd1 = get_sd(pos1) * u_base_modifier_params.sd_scale;
	const float msd2 = get_sd(pos2) * u_base_modifier_params.sd_scale;
	const float msd3 = get_sd(pos3) * u_base_modifier_params.sd_scale;

	if (u_base_modifier_params.operation == 0) {
		sd0 = sd_smooth_union(sd0, msd0, u_base_modifier_params.smoothness);
		sd1 = sd_smooth_union(sd1, msd1, u_base_modifier_params.smoothness);
		sd2 = sd_smooth_union(sd2, msd2, u_base_modifier_params.smoothness);
		sd3 = sd_smooth_union(sd3, msd3, u_base_modifier_params.smoothness);

	} else if (u_base_modifier_params.operation == 1) {
		sd0 = sd_smooth_subtract(sd0, msd0, u_base_modifier_params.smoothness);
		sd1 = sd_smooth_subtract(sd1, msd1, u_base_modifier_params.smoothness);
		sd2 = sd_smooth_subtract(sd2, msd2, u_base_modifier_params.smoothness);
		sd3 = sd_smooth_subtract(sd3, msd3, u_base_modifier_params.smoothness);

	} else {
		sd0 = msd0;
		sd1 = msd1;
		sd2 = msd2;
		sd3 = msd3;
	}

	u_out_sd.values[sdi] = sd0;
	u_out_sd.values[sdi + 1] = sd1;
	u_out_sd.values[sdi + 2] = sd2;
	u_out_sd.values[sdi + 3] = sd3;
}
