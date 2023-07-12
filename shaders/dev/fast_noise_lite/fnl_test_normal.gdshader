shader_type canvas_item;

#include "fast_noise_lite.gdshaderinc"

void fragment() {
	// Some test
	fnl_state state = fnlCreateState(1337);
	state.noise_type = FNL_NOISE_OPENSIMPLEX2;
	state.fractal_type = FNL_FRACTAL_RIDGED;
	state.octaves = 3;
	state.gain = 0.5;
	vec2 pos2d = UV * 100.0;
	vec3 pos = vec3(pos2d.x, pos2d.y, 1.0*TIME);
	float ns = 1.0;
	float n0 = fnlGetNoise3D(state, pos.x, pos.y, pos.z);
	float n1 = fnlGetNoise3D(state, pos.x+ns, pos.y, pos.z);
	float n2 = fnlGetNoise3D(state, pos.x, pos.y+ns, pos.z);
	float n3 = fnlGetNoise3D(state, pos.x, pos.y+ns, pos.z + ns);
	vec3 nm = normalize(vec3(n1 - n0, n2 - n0, n3 - n0));
	vec3 enm = nm * 0.5 + 0.5;
	COLOR = vec4(enm, 1.0);
}
