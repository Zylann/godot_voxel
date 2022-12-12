#[compute]
#version 450

// Takes a list of positions and evaluates a signed distance field in 4 locations around them.
// The 4 locations are picked such that the result can be used to compute a gradient.
// The result is then applied on top of previous values using a specified operation.

// This shader may be dispatched multiple times for each source of voxel data that we may combine for a given chunk.

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict buffer PositionBuffer {
	// X, Y, Z is hit position
	// W is integer triangle index
	vec4 values[];
} u_positions;

layout (set = 0, binding = 1, std430) restrict buffer Params {
	int tile_size_pixels;
	float pixel_world_step;
	int operation;
} u_params;

layout (set = 0, binding = 2, std430) restrict buffer InSDBuffer {
	// 4 values per index
	float values[];
} u_in_sd;

layout (set = 0, binding = 3, std430) restrict buffer OutSDBuffer {
	// 4 values per index
	float values[];
} u_out_sd;

// <PLACEHOLDER_SECTION>
float get_sd(vec3 pos) {
	return 0.0;
}
// </PLACEHOLDER_SECTION>

void main() {
	const ivec2 pixel_pos_in_tile = ivec2(gl_GlobalInvocationID.xy);
	const int tile_index = int(gl_GlobalInvocationID.z);

	const int index = pixel_pos_in_tile.x + pixel_pos_in_tile.y * u_params.tile_size_pixels 
		+ tile_index * u_params.tile_size_pixels * u_params.tile_size_pixels;

	if (u_positions.values[index].w == -1.0) {
		return;
	}

	const vec3 pos0 = u_positions.values[index].xyz;
	const vec3 pos1 = pos0 + vec3(u_params.pixel_world_step, 0.0, 0.0);
	const vec3 pos2 = pos0 + vec3(0.0, u_params.pixel_world_step, 0.0);
	const vec3 pos3 = pos0 + vec3(0.0, 0.0, u_params.pixel_world_step);

	const int sdi = index * 4;

	float sd0 = u_in_sd.values[sdi];
	float sd1 = u_in_sd.values[sdi + 1];
	float sd2 = u_in_sd.values[sdi + 2];
	float sd3 = u_in_sd.values[sdi + 3];

	const float msd0 = get_sd(pos0);
	const float msd1 = get_sd(pos1);
	const float msd2 = get_sd(pos2);
	const float msd3 = get_sd(pos3);

	if (u_params.operation == 0) {
		// Union
		sd0 = min(msd0, sd0);
		sd1 = min(msd1, sd1);
		sd2 = min(msd2, sd2);
		sd3 = min(msd3, sd3);

	} else if (u_params.operation == 1) {
		// Subtract
		sd0 = max(msd0, -sd0);
		sd1 = max(msd1, -sd1);
		sd2 = max(msd2, -sd2);
		sd3 = max(msd3, -sd3);

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
