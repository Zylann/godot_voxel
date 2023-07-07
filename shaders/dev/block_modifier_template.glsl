#[compute]
#version 450

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict readonly buffer Params {
	vec3 origin_in_voxels;
	float voxel_size;
	ivec3 block_size;
	int buffer_offset;
} u_params;

// SDF is modified in-place
layout (set = 0, binding = 1, std430) restrict buffer InSDBuffer {
	float values[];
} u_inout_sd;

// Parameters common to all modifiers.
// Keeping the same binding number as other shader types, to simplify usage in C++
layout (set = 0, binding = 4, std430) restrict readonly buffer BaseModifierParams {
	mat4 world_to_model;
	int operation;
	float smoothness;
	float sd_scale;
} u_base_modifier_params;

// <PLACEHOLDER>
float get_sd(vec3 pos) {
	return 0.0;
}
// </PLACEHOLDER>

float sd_smooth_union(float a, float b, float s) {
	const float h = clamp(0.5 + 0.5 * (b - a) / s, 0.0, 1.0);
	return mix(b, a, h) - s * h * (1.0 - h);
}

// Inverted a and b because it subtracts SDF a from SDF b
float sd_smooth_subtract(float b, float a, float s) {
	const float h = clamp(0.5 - 0.5 * (b + a) / s, 0.0, 1.0);
	return mix(b, -a, h) + s * h * (1.0 - h);
}

int get_zxy_index(ivec3 pos, ivec3 size) {
	return pos.y + size.y * (pos.x + size.x * pos.z);
}

void main() {
	const ivec3 rpos = ivec3(gl_GlobalInvocationID.xyz);

	// The output buffer might not have a 3D size multiple of our group size.
	// Some of the parallel executions will not do anything.
	if (rpos.x >= u_params.block_size.x || rpos.y >= u_params.block_size.y || rpos.z >= u_params.block_size.z) {
		return;
	}

	vec3 pos = u_params.origin_in_voxels + vec3(rpos) * u_params.voxel_size;

	pos = (u_base_modifier_params.world_to_model * vec4(pos, 1.0)).xyz;

	const int index = get_zxy_index(rpos, u_params.block_size) + u_params.buffer_offset;

	float sd = u_inout_sd.values[index];

	const float msd = get_sd(pos) * u_base_modifier_params.sd_scale;

	if (u_base_modifier_params.operation == 0) {
		sd = sd_smooth_union(sd, msd, u_base_modifier_params.smoothness);

	} else if (u_base_modifier_params.operation == 1) {
		sd = sd_smooth_subtract(sd, msd, u_base_modifier_params.smoothness);

	} else {
		sd = msd;
	}

	u_inout_sd.values[index] = sd;
}
