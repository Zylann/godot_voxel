// Generated file

// clang-format off
const char *g_modifier_mesh_shader_snippet = 
"\n"
"layout (set = 0, binding = 5) restrict readonly buffer ShapeParams {\n"
"	vec3 model_to_buffer_translation;\n"
"	vec3 model_to_buffer_scale;\n"
"	float isolevel;\n"
"} u_shape_params;\n"
"\n"
"layout (set = 0, binding = 6) uniform sampler3D u_sd_buffer;\n"
"\n"
"float get_sd(vec3 pos) {\n"
"	pos = u_shape_params.model_to_buffer_scale * (u_shape_params.model_to_buffer_translation + pos);\n"
"	return texture(u_sd_buffer, pos).r - u_shape_params.isolevel;\n"
"}\n"
"\n";
// clang-format on
