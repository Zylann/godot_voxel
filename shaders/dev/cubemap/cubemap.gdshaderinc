const int SIDE_POSITIVE_X = 0;
const int SIDE_NEGATIVE_X = 1;
const int SIDE_POSITIVE_Y = 2;
const int SIDE_NEGATIVE_Y = 3;
const int SIDE_POSITIVE_Z = 4;
const int SIDE_NEGATIVE_Z = 5;

vec3 cm_get_uv_from_xyz(vec3 p) {
	vec3 ap = abs(p);
	float ma;
	vec2 uv;
	int f;

	if (ap.z >= ap.x && ap.z >= ap.y) {
		f = p.z < 0.0 ? SIDE_NEGATIVE_Z : SIDE_POSITIVE_Z;
		// Protect from NaN when sampling at (0,0,0)
		ma = 0.5 / max(ap.z, 0.0001);
		uv = vec2(p.z < 0.0 ? -p.x : p.x, -p.y);

	} else if (ap.y >= ap.x) {
		f = p.y < 0. ? SIDE_NEGATIVE_Y : SIDE_POSITIVE_Y;
		ma = 0.5 / max(ap.y, 0.0001);
		uv = vec2(p.x, p.y < 0.f ? -p.z : p.z);

	} else {
		f = p.x < 0.0 ? SIDE_NEGATIVE_X : SIDE_POSITIVE_X;
		ma = 0.5 / max(ap.x, 0.0001);
		uv = vec2(p.x < 0.0 ? p.z : -p.z, -p.y);
	}

	return vec3(uv * ma + vec2(0.5), float(f));
}

// Calculates the cubic 2D Hermite interpolation within a quad of 4 points with known derivatives
float interpolate_hermite_2d(
		const vec2 r, // Relative position within the 2x2 quad
		const vec2 d, // Distance between the points (may default to (1,1))
		// Point values and their derivatives
		// x: value, y: x-derivative, z: y-derivative
		const vec3 t00,
		const vec3 t10,
		const vec3 t01,
		const vec3 t11
) {
	const float rx2 = r.x * r.x;
	const float ry2 = r.y * r.y;
	const float rx3 = rx2 * r.x;
	const float ry3 = ry2 * r.y;

	const float a0x = 2.f * rx3 - 3.f * rx2 + 1.f;
	const float a0y = 2.f * ry3 - 3.f * ry2 + 1.f;
	const float a1x = -2.f * rx3 + 3.f * rx2;
	const float a1y = -2.f * ry3 + 3.f * ry2;
	const float b0x = d.x * (rx3 - 2.f * rx2 + r.x);
	const float b0y = d.y * (ry3 - 2.f * ry2 + r.y);
	const float b1x = d.x * (rx3 - rx2);
	const float b1y = d.y * (ry3 - ry2);

	const float a00 = a0x * a0y;
	const float a10 = a1x * a0y;
	const float a01 = a0x * a1y;
	const float a11 = a1x * a1y;

	const vec2 b00 = vec2(b0x * a0y, a0x * b0y);
	const vec2 b10 = vec2(b1x * a0y, a1x * b0y);
	const vec2 b01 = vec2(b0x * a1y, a0x * b1y);
	const vec2 b11 = vec2(b1x * a1y, a1x * b1y);

	return //
			t00.x * a00 + //
			t10.x * a10 + //
			t01.x * a01 + //
			t11.x * a11 + //
			dot(t00.yz, b00) + //
			dot(t10.yz, b10) + //
			dot(t01.yz, b01) + //
			dot(t11.yz, b11);
}

float cm_sample_hermite(sampler2DArray tex, vec3 position, vec2 derivative_decoding_params) {
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

	vec2 d00 = derivative_decoding_params.x * c00.gb + derivative_decoding_params.y;
	vec2 d10 = derivative_decoding_params.x * c10.gb + derivative_decoding_params.y;
	vec2 d01 = derivative_decoding_params.x * c01.gb + derivative_decoding_params.y;
	vec2 d11 = derivative_decoding_params.x * c11.gb + derivative_decoding_params.y;

	//vec4 c = mix(mix(c00, c10, pf.x), mix(c01, c11, pf.x), pf.y);
	float c = interpolate_hermite_2d(pf, vec2(1.0),
		vec3(c00.r, d00),
		vec3(c10.r, d10),
		vec3(c01.r, d01),
		vec3(c11.r, d11)
	);

	return c;
}
