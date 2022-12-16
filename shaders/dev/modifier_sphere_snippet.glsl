#[compute]
#version 450

// <SNIPPET>

layout (set = 0, binding = 5) restrict readonly buffer ShapeParams {
	// Center not necessary, transform is applied in common shader code
	//vec3 center;
	float radius;
} u_shape_params;

float get_sd(vec3 pos) {
	return length(pos) - u_shape_params.radius;
}

// </SNIPPET>

void main() {}

