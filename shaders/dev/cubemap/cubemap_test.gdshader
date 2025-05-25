shader_type spatial;

#include "cubemap.gdshaderinc"

vec4 cm_sample_default(sampler2DArray tex, vec3 position) {
	vec3 uv = cm_get_uv_from_xyz(position);
	vec4 col = texture(tex, uv);
	return col;
}

float cm_sample_linear(sampler2DArray tex, vec3 position) {
	vec3 uv_norm = cm_get_uv_from_xyz(position);
	int side = int(uv_norm.z);
	vec2 uv_pixf = uv_norm.xy * vec2(textureSize(tex, 0).xy - ivec2(2)) + vec2(0.5);
	ivec2 p0 = ivec2(floor(uv_pixf));
	ivec2 p1 = p0 + ivec2(1);
	vec2 pf = uv_pixf - floor(uv_pixf);

	vec4 c00 = texelFetch(tex, ivec3(p0.x, p0.y, side), 0);
	vec4 c10 = texelFetch(tex, ivec3(p1.x, p0.y, side), 0);
	vec4 c01 = texelFetch(tex, ivec3(p0.x, p1.y, side), 0);
	vec4 c11 = texelFetch(tex, ivec3(p1.x, p1.y, side), 0);

	vec2 d00 = c00.gb;
	vec2 d10 = c10.gb;
	vec2 d01 = c01.gb;
	vec2 d11 = c11.gb;

	vec4 c = mix(mix(c00, c10, pf.x), mix(c01, c11, pf.x), pf.y);

	return c.r;
}



uniform sampler2DArray u_cubemap : filter_nearest;

varying vec3 v_pos;

void vertex() {
	v_pos = VERTEX;
}

void fragment() {
	float r = cm_sample_hermite(u_cubemap, v_pos);
	//float r = cm_sample_linear(u_cubemap, v_pos);
	//r = clamp(r, 0.0, 1.0);
	r = r*r;
	ALBEDO = vec3(r, r, r);
	//ALBEDO = cm_sample_default(u_cubemap, v_pos).rgb;
}
