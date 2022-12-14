#[compute]
#version 450

// Takes a list of positions and evaluates a signed distance field in 4 locations around them.
// The 4 locations are picked such that the result can be used to compute a gradient.
// This is similar to a modifier except there is no operation applied.
// Values are generated as a base, so the shader is simpler.

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict readonly buffer PositionBuffer {
	// X, Y, Z is hit position
	// W is integer triangle index
	vec4 values[];
} u_positions;

layout (set = 0, binding = 1, std430) restrict readonly buffer Params {
	int tile_size_pixels;
	float pixel_world_step;
} u_params;

layout (set = 0, binding = 2, std430) restrict writeonly buffer OutSDBuffer {
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

	u_out_sd.values[sdi] = get_sd(pos0);
	u_out_sd.values[sdi + 1] = get_sd(pos1);
	u_out_sd.values[sdi + 2] = get_sd(pos2);
	u_out_sd.values[sdi + 3] = get_sd(pos3);
}
