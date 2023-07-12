shader_type canvas_item;

#include "fast_noise_lite.gdshaderinc"

void fragment() {
	fnl_state state = fnlCreateState(1337);
	state.domain_warp_type = FNL_DOMAIN_WARP_BASICGRID;
	state.domain_warp_amp = 0.05;
	state.fractal_type = FNL_FRACTAL_DOMAIN_WARP_PROGRESSIVE;
	//state.fractal_type = FNL_FRACTAL_DOMAIN_WARP_INDEPENDENT;
	state.octaves = 3;
	state.gain = 0.5;
	state.frequency = 4.0;
	vec2 uv = UV;
	vec2 time_disp = vec2(TIME * 0.2, 0.0);
	uv += time_disp;
	fnlDomainWarp2D(state, uv.x, uv.y);
	uv -= time_disp;
	COLOR = texture(TEXTURE, uv);
//	COLOR = vec4(0.5 + 0.5 * uv, 0.0, 1.0);
}
