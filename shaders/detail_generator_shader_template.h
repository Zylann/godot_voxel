// Generated file

// clang-format off
const char *g_detail_generator_shader_template_0 = 
"#version 450\n"
"\n"
"// Takes a list of positions and evaluates a signed distance field in 4 locations around them.\n"
"// The 4 locations are picked such that the result can be used to compute a gradient.\n"
"// This is similar to a modifier except there is no operation applied.\n"
"// Values are generated as a base, so the shader is simpler.\n"
"\n"
"layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;\n"
"\n"
"layout (set = 0, binding = 0, std430) restrict readonly buffer PositionBuffer {\n"
"	// X, Y, Z is hit position\n"
"	// W is integer triangle index\n"
"	vec4 values[];\n"
"} u_positions;\n"
"\n"
"layout (set = 0, binding = 1, std430) restrict readonly buffer Params {\n"
"	int tile_size_pixels;\n"
"	float pixel_world_step;\n"
"} u_params;\n"
"\n"
"layout (set = 0, binding = 2, std430) restrict writeonly buffer OutSDBuffer {\n"
"	// 4 values per index\n"
"	float values[];\n"
"} u_out_sd;\n"
"\n";
// clang-format on

// clang-format off
const char *g_detail_generator_shader_template_1 = 
"\n"
"void main() {\n"
"	const ivec2 pixel_pos_in_tile = ivec2(gl_GlobalInvocationID.xy);\n"
"	const int tile_index = int(gl_GlobalInvocationID.z);\n"
"\n"
"	const int index = pixel_pos_in_tile.x + pixel_pos_in_tile.y * u_params.tile_size_pixels \n"
"		+ tile_index * u_params.tile_size_pixels * u_params.tile_size_pixels;\n"
"\n"
"	if (u_positions.values[index].w == -1.0) {\n"
"		return;\n"
"	}\n"
"\n"
"	const vec3 pos0 = u_positions.values[index].xyz;\n"
"	const vec3 pos1 = pos0 + vec3(u_params.pixel_world_step, 0.0, 0.0);\n"
"	const vec3 pos2 = pos0 + vec3(0.0, u_params.pixel_world_step, 0.0);\n"
"	const vec3 pos3 = pos0 + vec3(0.0, 0.0, u_params.pixel_world_step);\n"
"\n"
"	const int sdi = index * 4;\n"
"\n"
"	u_out_sd.values[sdi] = get_sd(pos0);\n"
"	u_out_sd.values[sdi + 1] = get_sd(pos1);\n"
"	u_out_sd.values[sdi + 2] = get_sd(pos2);\n"
"	u_out_sd.values[sdi + 3] = get_sd(pos3);\n"
"}\n"
"\n";
// clang-format on
