#[compute]
#version 450

// Takes a mesh and a list of tiles, where each tile corresponds to a cubic cell of the mesh.
// Each cell may contain a few triangles of the mesh,
// and tiles are oriented with an axis such that they are facing as much triangles as possible.
// We cast a ray from each pixel of each tile to triangles, to find world-space positions.
// Hit positions and triangle indices will be used to evaluate voxel data at these positions,
// which will in turn be used to bake a texture.

layout (local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

layout (set = 0, binding = 0, std430) restrict readonly buffer MeshVertices {
	vec3 data[];
} u_vertices;

layout (set = 0, binding = 1, std430) restrict readonly buffer MeshIndices {
	int data[];
} u_indices;

layout (set = 0, binding = 2, std430) restrict readonly buffer CellTris {
	// List of triangle indices.
	// Grouped in chunks corresponding to triangles within a tile.
	// Each chunk can have up to 5 triangle indices.
	int data[];
} u_cell_tris;

layout (set = 0, binding = 3, std430) restrict readonly buffer AtlasInfo {
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

layout (set = 0, binding = 4, std430) restrict readonly buffer Params {
	vec3 block_origin_world;
	// How big is a pixel of the atlas in world space
	float pixel_world_step;
	int tile_size_pixels;
} u_params;

layout (set = 0, binding = 5, std430) restrict writeonly buffer HitBuffer {
	// X, Y, Z is hit position
	// W is integer triangle index
	// Index is `pixel_pos_in_tile.x + pixel_pos_in_tile.y * tile_resolution + tile_index * (tile_resolution ^ 2)`
	vec4 positions[];
} u_hits;

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

void main() {
	const int tile_index = int(gl_GlobalInvocationID.z);

	const ivec2 tile_data = u_tile_data.data[tile_index];
	const int tri_count = (tile_data.y >> 4) & 0x7;

	if (tri_count == 0) {
		return;
	}

	const ivec2 pixel_pos_in_tile = ivec2(gl_GlobalInvocationID.xy);

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

	const vec2 pos_in_tile = u_params.pixel_world_step * vec2(pixel_pos_in_tile);
	const vec3 ray_origin_mesh = cell_origin_mesh
		 - 1.01 * ray_dir * cell_size_world
		 + pos_in_tile.x * dx + pos_in_tile.y * dy;

	// Find closest hit triangle
	const float no_hit_distance = 999999.0;
	float nearest_hit_distance = no_hit_distance;
	int nearest_hit_tri_index = -1;
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
			nearest_hit_tri_index = tri_index;
		}
	}

	const int index = pixel_pos_in_tile.x + pixel_pos_in_tile.y * u_params.tile_size_pixels 
		+ tile_index * u_params.tile_size_pixels * u_params.tile_size_pixels;

	if (nearest_hit_tri_index != -1) {
		const vec3 hit_pos_world = ray_origin_mesh + ray_dir * nearest_hit_distance + u_params.block_origin_world;
		u_hits.positions[index] = vec4(hit_pos_world, nearest_hit_tri_index);
	} else {
		u_hits.positions[index] = vec4(0.0, 0.0, 0.0, -1.0);
	}
}
