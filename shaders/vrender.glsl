#[compute]
#version 450

layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// TODO When should I use `restrict` or not?

// Output image.
// For now we write only to RGB8, but according to
// https://www.khronos.org/opengl/wiki/Layout_Qualifier_(GLSL),
// there doesn't seem to be a format for RGB8.
// So maybe we could use A for something custom in the future.
layout (set = 0, binding = 0, rgba8ui) writeonly uniform uimage2D u_target_image;

layout (set = 0, binding = 1, std430) restrict buffer MeshVertices {
	vec3 data[];
} u_vertices;

layout (set = 0, binding = 2, std430) restrict buffer MeshIndices {
	int data[];
} u_indices;

layout (set = 0, binding = 3, std430) restrict buffer CellTris {
	// List of triangle indices.
	// Grouped in chunks corresponding to triangles within a tile.
	// Each chunk can have up to 5 triangle indices.
	int data[];
} u_cell_tris;

layout (set = 0, binding = 4, std430) restrict buffer AtlasInfo {
	// [tile index] => cell info
	// X:
	// Packed 8-bit coordinates of the cell.
	// Y:
	// aaaaaaaa aaaaaaaa aaaaaaaa 0bbb00cc
	// a: 24-bit index into `u_cell_tris.data` array.
	// b: 3-bit number of triangles.
	// c: 2-bit projection direction (0:X, 1:Y, 2:Z)
	// Global invocation X and Y tell which pixel we are in.
	ivec2 data[];
} u_tile_data;

layout (set = 0, binding = 5, std430) restrict buffer Params {
	vec3 block_origin_world;
	// How big is a pixel of the atlas in world space
	float pixel_world_step;
	// cos(max_deviation_angle)
	float max_deviation_cosine;
	// sin(max_deviation_angle)
	float max_deviation_sine;

	int tile_size_pixels;
	int tiles_x;
	int tiles_y;
} u_params;

// This part contains functions we expect from the GLSL version of the voxel generator.
// <PLACEHOLDER_SECTION>
float generate_sdf(vec3 pos) {
	return 0.0;
}
// </PLACEHOLDER_SECTION>

float get_sd(vec3 pos) {
	return generate_sdf(pos);
}

vec3 get_sd_normal(vec3 pos, float s) {
	float sd000 = get_sd(pos);
	float sd100 = get_sd(pos + vec3(s, 0.0, 0.0));
	float sd010 = get_sd(pos + vec3(0.0, s, 0.0));
	float sd001 = get_sd(pos + vec3(0.0, 0.0, s));
	vec3 normal = normalize(vec3(sd100 - sd000, sd010 - sd000, sd001 - sd000));
	return normal;
}

const int TRI_NO_INTERSECTION = 0;
const int TRI_PARALLEL = 1;
const int TRI_INTERSECTION = 2;

int ray_intersects_triangle(vec3 p_from, vec3 p_dir, vec3 p_v0, vec3 p_v1, vec3 p_v2, out float out_distance) {
	const vec3 e1 = p_v1 - p_v0;
	const vec3 e2 = p_v2 - p_v0;
	const vec3 h = cross(p_dir, e2);
	const float a = dot(e1, h);

	if (abs(a) < 0.00001) {
		out_distance = -1.0;
		return TRI_PARALLEL;
	}

	const float f = 1.0f / a;

	const vec3 s = p_from - p_v0;
	const float u = f * dot(s, h);

	if ((u < 0.0) || (u > 1.0)) {
		out_distance = -1.0;
		return TRI_NO_INTERSECTION;
	}

	const vec3 q = cross(s, e1);

	const float v = f * dot(p_dir, q);

	if ((v < 0.0) || (u + v > 1.0)) {
		out_distance = -1.0;
		return TRI_NO_INTERSECTION;
	}

	// At this stage we can compute t to find out where
	// the intersection point is on the line.
	const float t = f * dot(e2, q);

	if (t > 0.00001) { // ray intersection
		//r_res = p_from + p_dir * t;
		out_distance = t;
		return TRI_INTERSECTION;

	} else { // This means that there is a line intersection but not a ray intersection.
		out_distance = -1.0;
		return TRI_NO_INTERSECTION;
	}
}

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
	const ivec2 pixel_pos = ivec2(gl_GlobalInvocationID.xy);

	const ivec2 tile_pos = pixel_pos / u_params.tile_size_pixels;
	const int tile_index = tile_pos.x + tile_pos.y * u_params.tiles_x;

	const ivec2 tile_data = u_tile_data.data[tile_index];
	const int tri_count = (tile_data.y >> 4) & 0x7;

	if (tri_count == 0) {
		//imageStore(u_target_image, pixel_pos, ivec4(255,0,255,255));
		return;
	}

	const int tri_info_begin = tile_data.y >> 8;
	const int projection = tile_data.y & 0x3;

	const int packed_cell_pos = tile_data.x;//u_tile_cell_positions.data[tile_index];
	const vec3 cell_pos_cells = vec3(
		packed_cell_pos & 0xff,
		(packed_cell_pos >> 8) & 0xff,
		(packed_cell_pos >> 16) & 0xff
	);
	const float cell_size_world = u_params.pixel_world_step * float(u_params.tile_size_pixels);
	const vec3 cell_origin_mesh = cell_size_world * cell_pos_cells;

	// Choose a basis where Z is the axis we cast the ray. X and Y are lateral axes of the tile.
	const vec3 ray_dir = vec3(float(projection == 0), float(projection == 1), float(projection == 2));
	const vec3 dx = vec3(float(projection == 1 || projection == 2), 0.0, float(projection == 0));
	const vec3 dy = vec3(0.0, float(projection == 0 || projection == 2), float(projection == 1));

	const ivec2 pixel_pos_in_tile = pixel_pos - tile_pos * u_params.tile_size_pixels;
	const vec2 pos_in_tile = u_params.pixel_world_step * vec2(pixel_pos_in_tile);
	const vec3 ray_origin_mesh = cell_origin_mesh
		 - 1.01 * ray_dir * cell_size_world
		 + pos_in_tile.x * dx + pos_in_tile.y * dy;

	// Find closest hit triangle
	const float no_hit_distance = 999999.0;
	float nearest_hit_distance = no_hit_distance;
	vec3 tri_v0;
	vec3 tri_v1;
	vec3 tri_v2;
	for (int i = 0; i < tri_count; ++i) {
		const int tri_index = u_cell_tris.data[tri_info_begin + i];
		const int ii0 = tri_index * 3;
		
		const int i0 = u_indices.data[ii0];
		const int i1 = u_indices.data[ii0 + 1];
		const int i2 = u_indices.data[ii0 + 2];

		const vec3 v0 = u_vertices.data[i0];
		const vec3 v1 = u_vertices.data[i1];
		const vec3 v2 = u_vertices.data[i2];

		float hit_distance;
		const int intersection_result = ray_intersects_triangle(ray_origin_mesh, ray_dir, v0, v1, v2, hit_distance);

		if (intersection_result == TRI_INTERSECTION && hit_distance < nearest_hit_distance) {
			nearest_hit_distance = hit_distance;
			tri_v0 = v0;
			tri_v1 = v1;
			tri_v2 = v2;
		}
	}

	ivec4 col = ivec4(127, 127, 127, 255);

	if (nearest_hit_distance != no_hit_distance) {
		const vec3 hit_pos_world = ray_origin_mesh + ray_dir * nearest_hit_distance + u_params.block_origin_world;

		const vec3 sd_normal = get_sd_normal(hit_pos_world, u_params.pixel_world_step);
		const vec3 tri_normal = get_triangle_normal(tri_v0, tri_v1, tri_v2);

		vec3 normal = sd_normal;

		// Clamp normal if it deviates too much
		const float tdot = dot(sd_normal, tri_normal);
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

		col = ivec4(255.0 * (vec3(0.5) + 0.5 * normal), 255);
	}

	imageStore(u_target_image, pixel_pos, col);
}
