// Generated file

// clang-format off
const char *g_modifier_sphere_shader_snippet = 
"\n"
"layout (set = 0, binding = 5) restrict readonly buffer ShapeParams {\n"
"	// Center not necessary, transform is applied in common shader code\n"
"	//vec3 center;\n"
"	float radius;\n"
"} u_shape_params;\n"
"\n"
"float get_sd(vec3 pos) {\n"
"	return length(pos) - u_shape_params.radius;\n"
"}\n"
"\n";
// clang-format on
