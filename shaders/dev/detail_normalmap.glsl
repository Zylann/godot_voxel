#[compute]
#version 450

// Takes signed distance values and computes a world-space normal from them.
// The normal's direction is then clamped based on triangles associated with each value.
// Then results are encoded in an output image.

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict readonly buffer SDBuffer {
	// 4 values per index
	float values[];
} u_in_sd;

layout (set = 0, binding = 1, std430) restrict readonly buffer MeshVertices {
	vec3 data[];
} u_vertices;

layout (set = 0, binding = 2, std430) restrict readonly buffer MeshIndices {
	int data[];
} u_indices;

layout (set = 0, binding = 3, std430) restrict readonly buffer HitBuffer {
	// X, Y, Z is hit position (UNUSED)
	// W is integer triangle index
	vec4 positions[];
} u_hits;

layout (set = 0, binding = 4, std430) restrict readonly buffer Params {
	int tile_size_pixels;
	int tiles_x;
	// cos(max_deviation_angle)
	float max_deviation_cosine;
	// sin(max_deviation_angle)
	float max_deviation_sine;
} u_params;

layout (set = 0, binding = 5, rgba8ui) writeonly uniform uimage2D u_target_image;

vec3 get_triangle_normal(vec3 v0, vec3 v1, vec3 v2) {
	vec3 e1 = v1 - v0;
	vec3 e2 = v2 - v0;
	return normalize(cross(e2, e1));
}

mat3 basis_from_axis_angle_cs(vec3 p_axis, float cosine, float sine) {
	// Rotation matrix from axis and angle, see
	// https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_angle

	mat3 cols;

	const vec3 axis_sq = vec3(p_axis.x * p_axis.x, p_axis.y * p_axis.y, p_axis.z * p_axis.z);
	cols[0][0] = axis_sq.x + cosine * (1.0f - axis_sq.x);
	cols[1][1] = axis_sq.y + cosine * (1.0f - axis_sq.y);
	cols[2][2] = axis_sq.z + cosine * (1.0f - axis_sq.z);

	const float t = 1 - cosine;

	float xyzt = p_axis.x * p_axis.y * t;
	float zyxs = p_axis.z * sine;
	cols[1][0] = xyzt - zyxs;
	cols[0][1] = xyzt + zyxs;

	xyzt = p_axis.x * p_axis.z * t;
	zyxs = p_axis.y * sine;
	cols[2][0] = xyzt + zyxs;
	cols[0][2] = xyzt - zyxs;

	xyzt = p_axis.y * p_axis.z * t;
	zyxs = p_axis.x * sine;
	cols[2][1] = xyzt - zyxs;
	cols[1][2] = xyzt + zyxs;

	return cols;
}

vec3 rotate_vec3_cs(const vec3 v, const vec3 axis, float cosine, float sine) {
	return basis_from_axis_angle_cs(axis, cosine, sine) * v;
}

void main() {
	const ivec2 pixel_pos_in_tile = ivec2(gl_GlobalInvocationID.xy);
	const int tile_index = int(gl_GlobalInvocationID.z);

	const int index = pixel_pos_in_tile.x + pixel_pos_in_tile.y * u_params.tile_size_pixels 
		+ tile_index * u_params.tile_size_pixels * u_params.tile_size_pixels;

	const ivec2 pixel_pos = 
		ivec2(tile_index % u_params.tiles_x, tile_index / u_params.tiles_x)
		* u_params.tile_size_pixels + pixel_pos_in_tile;

	const int tri_index = int(u_hits.positions[index].w);
	if (tri_index == -1) {
		imageStore(u_target_image, pixel_pos, ivec4(127, 127, 127, 255));
		return;
	}

	const int sdi = index * 4;

	const float sd000 = u_in_sd.values[sdi];
	const float sd100 = u_in_sd.values[sdi + 1];
	const float sd010 = u_in_sd.values[sdi + 2];
	const float sd001 = u_in_sd.values[sdi + 3];

	vec3 normal = normalize(vec3(sd100 - sd000, sd010 - sd000, sd001 - sd000));

	const int ii0 = tri_index * 3;

	const int i0 = u_indices.data[ii0];
	const int i1 = u_indices.data[ii0 + 1];
	const int i2 = u_indices.data[ii0 + 2];

	const vec3 v0 = u_vertices.data[i0];
	const vec3 v1 = u_vertices.data[i1];
	const vec3 v2 = u_vertices.data[i2];

	// In theory we could compute triangle normals once per triangle,
	// seems more efficient. But would it be significantly faster?
	const vec3 tri_normal = get_triangle_normal(v0, v1, v2);

	// Clamp normal if it deviates too much
	const float tdot = dot(normal, tri_normal);
	if (tdot < u_params.max_deviation_cosine) {
		if (tdot < -0.999) {
			normal = tri_normal;
		} else {
			const vec3 axis = normalize(cross(tri_normal, normal));
			normal = rotate_vec3_cs(tri_normal, axis, 
				u_params.max_deviation_cosine, 
				u_params.max_deviation_sine);
		}
	}

	const ivec4 col = ivec4(255.0 * (vec3(0.5) + 0.5 * normal), 255);

	imageStore(u_target_image, pixel_pos, col);
}
