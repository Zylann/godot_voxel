shader_type canvas_item;

#include "fast_noise_lite.gdshaderinc"

void fragment() {
	fnl_state state = fnlCreateState(1337);
	state.noise_type = FNL_NOISE_OPENSIMPLEX2;
	state.fractal_type = FNL_FRACTAL_FBM;
	state.octaves = 5;
	state.gain = 0.5;
	vec2 pos2d = UV * 100.0;
	vec3 pos = vec3(pos2d.x, pos2d.y, 10.0*TIME);
	float ns = 1.0;
	float n0 = 0.5 + 0.5 * fnlGetNoise3D(state, pos.x, pos.y, pos.z);
	COLOR = vec4(n0, n0, n0, 1.0);
}
