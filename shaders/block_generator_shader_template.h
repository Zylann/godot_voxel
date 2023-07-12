// Generated file

// clang-format off
const char *g_block_generator_shader_template_0 =
"#version 450\n"
"\n"
"layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;\n"
"\n"
"layout (set = 0, binding = 0, std430) restrict readonly buffer Params {\n"
"	vec3 origin_in_voxels;\n"
"	float voxel_size;\n"
"	ivec3 block_size;\n"
"	int buffer_offset;\n"
"} u_params;\n"
"\n"
"// Contains all outputs, each laid out in contiguous chunks of the same size.\n"
"// It must be indexed starting from `u_params.buffer_offset`.\n"
"layout (set = 0, binding = 1, std430) restrict writeonly buffer OutBuffer {\n"
"	float values[];\n"
"} u_out;\n"
"\n";
// clang-format on

// clang-format off
const char *g_block_generator_shader_template_1 =
"\n"
"int get_zxy_index(ivec3 pos, ivec3 size) {\n"
"	return pos.y + size.y * (pos.x + size.x * pos.z);\n"
"}\n"
"\n"
"int get_volume(ivec3 v) {\n"
"	return v.x * v.y * v.z;\n"
"}\n"
"\n"
"void main() {\n"
"	const ivec3 rpos = ivec3(gl_GlobalInvocationID.xyz);\n"
"	// The output buffer might not have a 3D size multiple of our group size.\n"
"	// Some of the parallel executions will not do anything.\n"
"	if (rpos.x >= u_params.block_size.x || rpos.y >= u_params.block_size.y || rpos.z >= u_params.block_size.z) {\n"
"		return;\n"
"	}\n"
"\n"
"	const int out_index = get_zxy_index(rpos, u_params.block_size) + u_params.buffer_offset;\n"
"\n"
"	// May be used by generated code for generators that have more than one output\n"
"	const int volume = get_volume(u_params.block_size);\n"
"\n"
"	const vec3 wpos = u_params.origin_in_voxels + vec3(rpos) * u_params.voxel_size;\n"
"	// float sd = get_sd(wpos);\n"
"	// u_out_sd.values[out_index] = sd;\n"
"\n";
// clang-format on

// clang-format off
const char *g_block_generator_shader_template_2 =
"}\n";
// clang-format on
