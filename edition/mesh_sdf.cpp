#include "mesh_sdf.h"
#include "../util/godot/funcs.h"
#include "../util/math/box3i.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h" // Debug

namespace zylann::voxel::mesh_sdf {

// Some papers for eventual improvements
// Jump flood
// https://www.comp.nus.edu.sg/%7Etants/jfa/i3d06.pdf
// GPU-based technique
// https://www.researchgate.net/profile/Bastian-Krayer/publication/332921884_Generating_signed_distance_fields_on_the_GPU_with_ray_maps/links/5d63d921299bf1f70b0de26b/Generating-signed-distance-fields-on-the-GPU-with-ray-maps.pdf
// In the future, maybe also gather gradients so it can be used efficiently with Dual Contouring?

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float get_max_sdf_variation(Vector3f min_pos, Vector3f max_pos, Vector3i res) {
	const Vector3f box_size = max_pos - min_pos;
	const Vector3f max_variation_v = box_size / to_vec3f(res);
	return math::max(max_variation_v.x, math::max(max_variation_v.y, max_variation_v.z));
}

enum Flag { //
	FLAG_NOT_VISITED,
	FLAG_VISITED
	//FLAG_FROZEN
};

void fix_sdf_sign_from_boundary(
		Span<float> sdf_grid, Span<uint8_t> flag_grid, Vector3i res, Vector3f min_pos, Vector3f max_pos) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(sdf_grid.size() == flag_grid.size());

	if (res.x == 0 || res.y == 0 || res.z == 0) {
		return;
	}

	std::vector<Vector3i> seeds;

	FixedArray<Vector3i, 6> dirs;
	dirs[0] = Vector3i(-1, 0, 0);
	dirs[1] = Vector3i(1, 0, 0);
	dirs[2] = Vector3i(0, -1, 0);
	dirs[3] = Vector3i(0, 1, 0);
	dirs[4] = Vector3i(0, 0, -1);
	dirs[5] = Vector3i(0, 0, 1);

	const float tolerance = 1.1f;
	const float max_variation = tolerance * get_max_sdf_variation(min_pos, max_pos, res);
	const float min_sd = max_variation * 2.f;

	seeds.push_back(Vector3i());
	flag_grid[0] = FLAG_VISITED;

	// Spread positive sign from boundary.
	// Has a bit of room for optimization, but profiling shows it accounts for a very small portion of time compared to
	// calculating the distance field.
	while (seeds.size() > 0) {
		const Vector3i pos = seeds.back();
		seeds.pop_back();
		const unsigned int loc = Vector3iUtil::get_zxy_index(pos, res);
		const float v = sdf_grid[loc];

		for (unsigned int dir = 0; dir < dirs.size(); ++dir) {
			const Vector3i npos = pos + dirs[dir];

			if (npos.x < 0 || npos.y < 0 || npos.z < 0 || npos.x >= res.x || npos.y >= res.y || npos.z >= res.z) {
				continue;
			}

			const unsigned int nloc = Vector3iUtil::get_zxy_index(npos, res);

			ZN_ASSERT(nloc < flag_grid.size());
			const uint8_t flag = flag_grid[nloc];
			if (flag == FLAG_VISITED) {
				continue;
			}

			flag_grid[nloc] = FLAG_VISITED;

			//ZN_ASSERT(nloc < sdf_grid.size());
			const float nv = sdf_grid[nloc];

			if ((nv > 0.f && nv < min_sd) || ((nv > 0.f) != (v > 0.f) && Math::abs(nv - v) < max_variation)) {
				// Too close to outer surface, or legit sign change occurs.
				// If we keep floodfilling close to surface or where the sign flips at low distances,
				// we would risk inverting the sign of the inside of the shape, removing all signedness.
				// However, if sign flips with a high distance variation, we definitely want to correct that.
				continue;
			}

			if (nv < 0.f) {
				sdf_grid[nloc] = -nv;
			}
			seeds.push_back(npos);
		}
	}
}

void fix_sdf_sign_from_boundary(Span<float> sdf_grid, Vector3i res, Vector3f min_pos, Vector3f max_pos) {
	std::vector<uint8_t> flag_grid;
	flag_grid.resize(sdf_grid.size(), FLAG_NOT_VISITED);
	fix_sdf_sign_from_boundary(sdf_grid, to_span(flag_grid), res, min_pos, max_pos);
}

void partition_triangles(
		int subdiv, Span<const Triangle> triangles, Vector3f min_pos, Vector3f max_pos, ChunkGrid &chunk_grid) {
	ZN_PROFILE_SCOPE();

	const Vector3f mesh_size = max_pos - min_pos;
	const float cs = math::max(mesh_size.x, math::max(mesh_size.y, mesh_size.z)) / subdiv;
	const Vector3i grid_min = to_vec3i(math::floor(min_pos / cs));
	const Vector3i grid_max = to_vec3i(math::ceil(max_pos / cs));

	chunk_grid.size = grid_max - grid_min;
	chunk_grid.chunks.resize(Vector3iUtil::get_volume(chunk_grid.size));
	chunk_grid.chunk_size = cs;
	chunk_grid.min_pos = min_pos;

	// Initialize chunk positions
	{
		Vector3i cpos;
		for (cpos.z = 0; cpos.z < chunk_grid.size.z; ++cpos.z) {
			for (cpos.x = 0; cpos.x < chunk_grid.size.x; ++cpos.x) {
				cpos.y = 0;
				unsigned int ci = Vector3iUtil::get_zxy_index(cpos, chunk_grid.size);

				for (; cpos.y < chunk_grid.size.y; ++cpos.y) {
					ZN_ASSERT(ci < chunk_grid.chunks.size());
					Chunk &chunk = chunk_grid.chunks[ci];

					chunk.pos = cpos;
					++ci;
				}
			}
		}
	}

	// Group triangles overlapping chunks
	{
		ZN_PROFILE_SCOPE_NAMED("Group triangles");

		for (unsigned int triangle_index = 0; triangle_index < triangles.size(); ++triangle_index) {
			const Triangle &t = triangles[triangle_index];

			const Vector3f tri_min_pos = math::min(t.v1, math::min(t.v2, t.v3));
			const Vector3f tri_max_pos = math::max(t.v1, math::max(t.v2, t.v3));

			const Vector3i tri_min_pos_i = to_vec3i(math::floor(tri_min_pos / cs)) - grid_min;
			const Vector3i tri_max_pos_i = to_vec3i(math::floor(tri_max_pos / cs)) - grid_min;

			Vector3i cpos;
			for (cpos.z = tri_min_pos_i.z; cpos.z <= tri_max_pos_i.z; ++cpos.z) {
				for (cpos.x = tri_min_pos_i.x; cpos.x <= tri_max_pos_i.x; ++cpos.x) {
					cpos.y = tri_min_pos_i.y;
					unsigned int ci = Vector3iUtil::get_zxy_index(cpos, chunk_grid.size);

					for (; cpos.y <= tri_max_pos_i.y; ++cpos.y) {
						ZN_ASSERT(ci < chunk_grid.chunks.size());
						Chunk &chunk = chunk_grid.chunks[ci];
						chunk.triangles.push_back(&t);
						++ci;
					}
				}
			}
		}
	}

	// Gather chunks containing triangles
	std::vector<const Chunk *> nonempty_chunks;
	for (auto it = chunk_grid.chunks.begin(); it != chunk_grid.chunks.end(); ++it) {
		const Chunk &chunk = *it;
		if (chunk.triangles.size() > 0) {
			nonempty_chunks.push_back(&chunk);
		}
	}

	// Find close chunks
	{
		ZN_PROFILE_SCOPE_NAMED("Group chunks");

		Vector3i cpos;
		for (cpos.z = 0; cpos.z < chunk_grid.size.z; ++cpos.z) {
			for (cpos.x = 0; cpos.x < chunk_grid.size.x; ++cpos.x) {
				cpos.y = 0;
				unsigned int ci = Vector3iUtil::get_zxy_index(cpos, chunk_grid.size);

				for (; cpos.y < chunk_grid.size.y; ++cpos.y, ++ci) {
					ZN_ASSERT(ci < chunk_grid.chunks.size());
					Chunk &chunk = chunk_grid.chunks[ci];

					// Find closest chunk

					const Chunk *closest_chunk = nullptr;
					// Distance is in chunks
					int closest_chunk_distance_squared = 0x0fffffff;

					if (chunk.triangles.size() > 0) {
						closest_chunk = &chunk;
						closest_chunk_distance_squared = 0;

					} else {
						for (auto it = nonempty_chunks.begin(); it != nonempty_chunks.end(); ++it) {
							const Chunk &nchunk = **it;
							const int distance_squared = (nchunk.pos - chunk.pos).length_squared();
							if (distance_squared < closest_chunk_distance_squared) {
								closest_chunk = &nchunk;
								closest_chunk_distance_squared = distance_squared;
							}
						}
					}

					ZN_ASSERT(closest_chunk != nullptr);

					// Find other close chunks slightly beyond the closest chunk.
					// This is to account for the fact the closest chunk might contain a triangle further away than
					// a closer triangle found in a farther chunk.

					const int margin_distance_squared =
							math::squared(sqrtf(closest_chunk_distance_squared) + constants::SQRT3);

					for (auto it = nonempty_chunks.begin(); it != nonempty_chunks.end(); ++it) {
						const Chunk &nchunk = **it;
						const int distance_squared = (nchunk.pos - chunk.pos).length_squared();
						// Note, this will include the closest chunk
						if (distance_squared <= margin_distance_squared) {
							chunk.near_chunks.push_back(&nchunk);
						}
					}

					// println(format("Chunk {} has {} / {} near non-empty chunks", chunk.pos, chunk.near_chunks.size(),
					// 		nonempty_chunks.size()));
				}
			}
		}
	}
}

/*
// Non-optimized version, suitable for single queries
float get_distance_to_triangle_squared(const Vector3f v1, const Vector3f v2, const Vector3f v3, const Vector3f p) {
	// https://iquilezles.org/articles/triangledistance/

	const Vector3f v21 = v2 - v1;
	const Vector3f v32 = v3 - v2;
	const Vector3f v13 = v1 - v3;

	const Vector3f p1 = p - v1;
	const Vector3f p2 = p - v2;
	const Vector3f p3 = p - v3;

	const Vector3f nor = v21.cross(v13);

	const float det = //
			signf(v21.cross(nor).dot(p1)) + //
			signf(v32.cross(nor).dot(p2)) + //
			signf(v13.cross(nor).dot(p3));

	if (det < 2.f) {
		// Outside of the prism: get distance to closest edge
		return math::min(
				math::min( //
						(v21 * math::clamp(v21.dot(p1) / v21.length_squared(), 0.f, 1.f) - p1).length_squared(),
						(v32 * math::clamp(v32.dot(p2) / v32.length_squared(), 0.f, 1.f) - p2).length_squared()),
				(v13 * math::clamp(v13.dot(p3) / v13.length_squared(), 0.f, 1.f) - p3).length_squared());
	} else {
		// Inside the prism: get distance to plane
		return math::squared(nor.dot(p1)) / nor.length_squared();
	}
}
*/

// Returns the distance from a point to a triangle, where some terms are precalculated.
// This may be preferred if the same triangles have to be queried many times.
float get_distance_to_triangle_squared_precalc(const Triangle &t, const Vector3f p) {
	// https://iquilezles.org/articles/triangledistance/

	const Vector3f p1 = p - t.v1;
	const Vector3f p2 = p - t.v2;
	const Vector3f p3 = p - t.v3;

	const float det = //
			math::sign(t.v21_cross_nor.dot(p1)) + //
			math::sign(t.v32_cross_nor.dot(p2)) + //
			math::sign(t.v13_cross_nor.dot(p3));

	if (det < 2.f) {
		// Outside of the prism: get distance to closest edge
		return math::min(
				math::min( //
						(t.v21 * math::clamp(t.v21.dot(p1) * t.inv_v21_length_squared, 0.f, 1.f) - p1).length_squared(),
						(t.v32 * math::clamp(t.v32.dot(p2) * t.inv_v32_length_squared, 0.f, 1.f) - p2)
								.length_squared()),
				(t.v13 * math::clamp(t.v13.dot(p3) * t.inv_v13_length_squared, 0.f, 1.f) - p3).length_squared());
	} else {
		// Inside the prism: get distance to plane
		return math::squared(t.nor.dot(p1)) * t.inv_nor_length_squared;
	}
}

void precalc_triangles(Span<Triangle> triangles) {
	ZN_PROFILE_SCOPE();
	for (size_t i = 0; i < triangles.size(); ++i) {
		Triangle &t = triangles[i];
		t.v21 = t.v2 - t.v1;
		t.v32 = t.v3 - t.v2;
		t.v13 = t.v1 - t.v3;
		t.nor = t.v21.cross(t.v13);
		t.v21_cross_nor = t.v21.cross(t.nor);
		t.v32_cross_nor = t.v32.cross(t.nor);
		t.v13_cross_nor = t.v13.cross(t.nor);
		t.inv_v21_length_squared = 1.f / t.v21.length_squared();
		t.inv_v32_length_squared = 1.f / t.v32.length_squared();
		t.inv_v13_length_squared = 1.f / t.v13.length_squared();
		t.inv_nor_length_squared = 1.f / t.nor.length_squared();
	}
}

unsigned int get_closest_triangle_precalc(const Vector3f pos, Span<const Triangle> triangles) {
	float min_distance_squared = 9999999.f;
	size_t closest_tri_index = triangles.size();

	for (size_t i = 0; i < triangles.size(); ++i) {
		const Triangle &t = triangles[i];
		//const float sqd = get_distance_to_triangle_squared(t.v1, t.v2, t.v3, pos);
		const float sqd = get_distance_to_triangle_squared_precalc(t, pos);

		if (sqd < min_distance_squared) {
			min_distance_squared = sqd;
			closest_tri_index = i;
		}
	}

	return closest_tri_index;
}

float get_mesh_signed_distance_at(const Vector3f pos, Span<const Triangle> triangles /*, bool debug = false*/) {
	float min_distance_squared = 9999999.f;
	size_t closest_tri_index = 0;

	for (size_t i = 0; i < triangles.size(); ++i) {
		const Triangle &t = triangles[i];
		//const float sqd = get_distance_to_triangle_squared(t.v1, t.v2, t.v3, pos);
		const float sqd = get_distance_to_triangle_squared_precalc(t, pos);

		// TODO What if two triangles of opposite directions share the same point?
		// If the distance comes from that point, it makes finding the sign ambiguous.
		// sometimes two triangles of opposite directions share an edge or point, and there is no quick way
		// to figure out which one must be taken.
		// For now this is worked around in a later pass with a floodfill.

		/*if (debug) {
			if (sqd < min_distance_squared + 0.01f) {
				const Vector3f plane_normal = t.v13.cross(t.v1 - t.v2).normalized();
				const float plane_d = plane_normal.dot(t.v1);
				float s;
				if (plane_normal.dot(pos) > plane_d) {
					s = 1.f;
				} else {
					s = -1.f;
				}
				println(format("Candidate tri {} with sqd {} sign {}", i, sqd, s));
			}
		}*/

		if (sqd < min_distance_squared) {
			/*if (debug) {
				println(format("Better tri {} with sqd {}", i, sqd));
			}*/
			min_distance_squared = sqd;
			closest_tri_index = i;
		}
	}

	const Triangle &ct = triangles[closest_tri_index];
	const float d = Math::sqrt(min_distance_squared);

	//if (p_dir == CLOCKWISE) {
	//const Vector3f plane_normal = (ct.v1 - ct.v3).cross(ct.v1 - ct.v2).normalized();
	const Vector3f plane_normal = ct.v13.cross(ct.v1 - ct.v2).normalized();
	//} else {
	//	normal = (p_point1 - p_point2).cross(p_point1 - p_point3);
	//}
	const float plane_d = plane_normal.dot(ct.v1);

	if (plane_normal.dot(pos) > plane_d) {
		return d;
	}
	return -d;
}

float get_mesh_signed_distance_at(const Vector3f pos, const ChunkGrid &chunk_grid) {
	float min_distance_squared = 9999999.f;
	const Triangle *closest_tri = nullptr;

	const Vector3i chunk_pos = to_vec3i(math::floor((pos - chunk_grid.min_pos) / chunk_grid.chunk_size));
	const unsigned chunk_index = Vector3iUtil::get_zxy_index(chunk_pos, chunk_grid.size);
	ZN_ASSERT(chunk_index < chunk_grid.chunks.size());
	const Chunk &chunk = chunk_grid.chunks[chunk_index];

	for (auto near_chunk_it = chunk.near_chunks.begin(); near_chunk_it != chunk.near_chunks.end(); ++near_chunk_it) {
		const Chunk &near_chunk = **near_chunk_it;

		for (auto tri_it = near_chunk.triangles.begin(); tri_it != near_chunk.triangles.end(); ++tri_it) {
			const Triangle &t = **tri_it;

			// TODO What if two triangles of opposite directions share the same point?
			// If the distance comes from that point, it makes finding the sign ambiguous.
			// sometimes two triangles of opposite directions share an edge or point, and there is no quick way
			// to figure out which one must be taken.
			// For now this is worked around in a later pass with a floodfill.

			//const float sqd = get_distance_to_triangle_squared(t.v1, t.v2, t.v3, pos);
			const float sqd = get_distance_to_triangle_squared_precalc(t, pos);

			if (sqd < min_distance_squared) {
				min_distance_squared = sqd;
				closest_tri = &t;
			}
		}
	}

	const float d = Math::sqrt(min_distance_squared);
	ZN_ASSERT(closest_tri != nullptr);

	//if (p_dir == CLOCKWISE) {
	//const Vector3f plane_normal = (ct.v1 - ct.v3).cross(ct.v1 - ct.v2).normalized();
	const Vector3f plane_normal = closest_tri->v13.cross(closest_tri->v1 - closest_tri->v2).normalized();
	//} else {
	//	normal = (p_point1 - p_point2).cross(p_point1 - p_point3);
	//}
	const float plane_d = plane_normal.dot(closest_tri->v1);

	if (plane_normal.dot(pos) > plane_d) {
		return d;
	}
	return -d;
}

struct GridToSpaceConverter {
	const Vector3i res;
	const Vector3f min_pos;
	const Vector3f mesh_size;
	const Vector3f half_cell_size;

	// Grid to space transform
	const Vector3f translation;
	const Vector3f scale;

	GridToSpaceConverter(Vector3i p_resolution, Vector3f p_min_pos, Vector3f p_mesh_size, Vector3f p_half_cell_size) :
			res(p_resolution),
			min_pos(p_min_pos),
			mesh_size(p_mesh_size),
			half_cell_size(p_half_cell_size),
			translation(min_pos + half_cell_size),
			scale(mesh_size / to_vec3f(res)) {}

	inline Vector3f operator()(const Vector3i grid_pos) const {
		return translation + scale * to_vec3f(grid_pos);
	}
};

struct Evaluator {
	Span<const Triangle> triangles;
	const GridToSpaceConverter grid_to_space;

	inline float operator()(const Vector3i grid_pos) const {
		return get_mesh_signed_distance_at(grid_to_space(grid_pos), triangles);
	}
};

struct EvaluatorCG {
	const ChunkGrid &chunk_grid;
	const GridToSpaceConverter grid_to_space;

	inline float operator()(const Vector3i grid_pos) const {
		return get_mesh_signed_distance_at(grid_to_space(grid_pos), chunk_grid);
	}
};

void mark_triangle_hull(Span<uint8_t> flag_grid, const Vector3i res, Span<const Triangle> triangles, Vector3f min_pos,
		Vector3f max_pos, uint8_t flag_value, int aabb_padding) {
	ZN_PROFILE_SCOPE();

	const Vector3f mesh_size = max_pos - min_pos;
	const Vector3f cell_size = mesh_size / Vector3f(res.x, res.y, res.z);
	const Evaluator eval{ triangles, GridToSpaceConverter(res, min_pos, mesh_size, cell_size * 0.5f) };

	const Vector3f inv_gts_scale = Vector3f(1.f) / eval.grid_to_space.scale;

	const Box3i grid_box(Vector3i(), res);

	for (unsigned int i = 0; i < triangles.size(); ++i) {
		const Triangle &t = triangles[i];

		const Vector3f aabb_min = math::min(t.v1, math::min(t.v2, t.v3));
		const Vector3f aabb_max = math::max(t.v1, math::max(t.v2, t.v3));

		// Space to grid
		const Vector3f aabb_min_g = inv_gts_scale * (aabb_min - eval.grid_to_space.translation);
		const Vector3f aabb_max_g = inv_gts_scale * (aabb_min - eval.grid_to_space.translation);

		const Box3i tbox = Box3i::from_min_max(to_vec3i(math::floor(aabb_min_g)), to_vec3i(math::ceil(aabb_max_g)))
								   .padded(aabb_padding)
								   .clipped(grid_box);

		tbox.for_each_cell_zxy([&flag_grid, eval, res, flag_value](const Vector3i &grid_pos) {
			const size_t i = Vector3iUtil::get_zxy_index(grid_pos, res);
			flag_grid[i] = flag_value;
		});
	}
}

void generate_mesh_sdf_approx_interp(Span<float> sdf_grid, const Vector3i res, Span<const Triangle> triangles,
		const Vector3f min_pos, const Vector3f max_pos) {
	ZN_PROFILE_SCOPE();

	static const float FAR_SD = 9999999.f;

	const Vector3f mesh_size = max_pos - min_pos;
	const Vector3f cell_size = mesh_size / Vector3f(res.x, res.y, res.z);
	const Evaluator eval{ triangles, GridToSpaceConverter(res, min_pos, mesh_size, cell_size * 0.5f) };

	const unsigned int node_size_po2 = 2;
	const unsigned int node_size_cells = 1 << node_size_po2;

	const Vector3i node_grid_size = math::ceildiv(res, node_size_cells) + Vector3i(1, 1, 1);
	const float node_size = node_size_cells * cell_size.length();
	const float node_subdiv_threshold = 0.6f * node_size;

	std::vector<float> node_grid;
	node_grid.resize(Vector3iUtil::get_volume(node_grid_size));

	// Fill SDF grid with far distances as "infinity", we'll use that to check if we computed it already
	sdf_grid.fill(FAR_SD);

	// Evaluate SDF at the corners of nodes
	Vector3i node_pos;
	for (node_pos.z = 0; node_pos.z < node_grid_size.z; ++node_pos.z) {
		for (node_pos.x = 0; node_pos.x < node_grid_size.x; ++node_pos.x) {
			for (node_pos.y = 0; node_pos.y < node_grid_size.y; ++node_pos.y) {
				const Vector3i gp000 = node_pos << node_size_po2;

				const size_t ni = Vector3iUtil::get_zxy_index(node_pos, node_grid_size);
				ZN_ASSERT(ni < node_grid.size());
				const float sd = eval(gp000);
				node_grid[ni] = sd;

				if (Box3i(Vector3i(), res).contains(gp000)) {
					const size_t i = Vector3iUtil::get_zxy_index(gp000, res);
					ZN_ASSERT(i < sdf_grid.size());
					sdf_grid[i] = sd;
				}
			}
		}
	}

	// Precompute flat-grid neighbor offsets
	const unsigned int ni000 = 0;
	const unsigned int ni100 = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 0), node_grid_size);
	const unsigned int ni010 = Vector3iUtil::get_zxy_index(Vector3i(0, 1, 0), node_grid_size);
	const unsigned int ni110 = Vector3iUtil::get_zxy_index(Vector3i(1, 1, 0), node_grid_size);
	const unsigned int ni001 = Vector3iUtil::get_zxy_index(Vector3i(0, 0, 1), node_grid_size);
	const unsigned int ni101 = Vector3iUtil::get_zxy_index(Vector3i(1, 0, 1), node_grid_size);
	const unsigned int ni011 = Vector3iUtil::get_zxy_index(Vector3i(0, 1, 1), node_grid_size);
	const unsigned int ni111 = Vector3iUtil::get_zxy_index(Vector3i(1, 1, 1), node_grid_size);

	// Then for every node
	for (node_pos.z = 0; node_pos.z < node_grid_size.z - 1; ++node_pos.z) {
		for (node_pos.x = 0; node_pos.x < node_grid_size.x - 1; ++node_pos.x) {
			for (node_pos.y = 0; node_pos.y < node_grid_size.y - 1; ++node_pos.y) {
				float ud = FAR_SD;

				const unsigned int ni = Vector3iUtil::get_zxy_index(node_pos, node_grid_size);

				// Get signed distance at each corner we computed earlier
				const float sd000 = node_grid[ni];
				const float sd100 = node_grid[ni + ni100];
				const float sd010 = node_grid[ni + ni010];
				const float sd110 = node_grid[ni + ni110];
				const float sd001 = node_grid[ni + ni001];
				const float sd101 = node_grid[ni + ni101];
				const float sd011 = node_grid[ni + ni011];
				const float sd111 = node_grid[ni + ni111];

				// Get smallest one
				ud = math::min(ud, Math::abs(sd000));
				ud = math::min(ud, Math::abs(sd100));
				ud = math::min(ud, Math::abs(sd010));
				ud = math::min(ud, Math::abs(sd110));
				ud = math::min(ud, Math::abs(sd001));
				ud = math::min(ud, Math::abs(sd101));
				ud = math::min(ud, Math::abs(sd011));
				ud = math::min(ud, Math::abs(sd111));

				const Box3i cell_box = Box3i(node_pos * node_size_cells, Vector3iUtil::create(node_size_cells))
											   .clipped(Box3i(Vector3i(), res));

				// If the minimum distance at the corners of the node is lower than the threshold,
				// subdivide the node.
				if (ud < node_subdiv_threshold) {
					// Full-res SDF
					cell_box.for_each_cell_zxy([&sdf_grid, eval, res](const Vector3i grid_pos) {
						const size_t i = Vector3iUtil::get_zxy_index(grid_pos, res);
						if (sdf_grid[i] != FAR_SD) {
							// Already computed
							return;
						}
						ZN_ASSERT(i < sdf_grid.size());
						sdf_grid[i] = eval(grid_pos);
					});
				} else {
					// We are far enough from the surface, approximate by interpolating corners
					const Vector3i cell_box_end = cell_box.pos + cell_box.size;
					Vector3i grid_pos;
					for (grid_pos.z = cell_box.pos.z; grid_pos.z < cell_box_end.z; ++grid_pos.z) {
						for (grid_pos.x = cell_box.pos.x; grid_pos.x < cell_box_end.x; ++grid_pos.x) {
							for (grid_pos.y = cell_box.pos.y; grid_pos.y < cell_box_end.y; ++grid_pos.y) {
								const size_t i = Vector3iUtil::get_zxy_index(grid_pos, res);
								if (sdf_grid[i] != FAR_SD) {
									// Already computed
									continue;
								}
								const Vector3f ipf = to_vec3f(grid_pos - cell_box.pos) / float(node_size_cells);
								const float sd = math::interpolate_trilinear(
										sd000, sd100, sd101, sd001, sd010, sd110, sd111, sd011, ipf);
								ZN_ASSERT(i < sdf_grid.size());
								sdf_grid[i] = sd;
							}
						}
					}
				}
			}
		}
	}
}

void generate_mesh_sdf_naive(Span<float> sdf_grid, const Vector3i res, const Box3i sub_box,
		Span<const Triangle> triangles, const Vector3f min_pos, const Vector3f max_pos) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(Box3i(Vector3i(), res).contains(sub_box));
	ZN_ASSERT(sdf_grid.size() == Vector3iUtil::get_volume(res));

	const Vector3f mesh_size = max_pos - min_pos;
	const Vector3f cell_size = mesh_size / Vector3f(res.x, res.y, res.z);
	const Evaluator eval{ triangles, GridToSpaceConverter(res, min_pos, mesh_size, cell_size * 0.5f) };

	Vector3i grid_pos;

	const Vector3i sub_box_end = sub_box.pos + sub_box.size;

	for (grid_pos.z = sub_box.pos.z; grid_pos.z < sub_box_end.z; ++grid_pos.z) {
		for (grid_pos.x = sub_box.pos.x; grid_pos.x < sub_box_end.x; ++grid_pos.x) {
			grid_pos.y = sub_box.pos.y;
			size_t grid_index = Vector3iUtil::get_zxy_index(grid_pos, res);

			for (; grid_pos.y < sub_box_end.y; ++grid_pos.y) {
				const float sd = eval(grid_pos);

				ZN_ASSERT(grid_index < sdf_grid.size());
				sdf_grid[grid_index] = sd;

				++grid_index;
			}
		}
	}

	// const uint64_t usec = profiling_clock.restart();
	// println(format("Spent {} usec", usec));
}

void generate_mesh_sdf_partitioned(Span<float> sdf_grid, const Vector3i res, const Box3i sub_box,
		const Vector3f min_pos, const Vector3f max_pos, const ChunkGrid &chunk_grid) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(Box3i(Vector3i(), res).contains(sub_box));
	ZN_ASSERT(sdf_grid.size() == Vector3iUtil::get_volume(res));

	const Vector3f mesh_size = max_pos - min_pos;
	const Vector3f cell_size = mesh_size / Vector3f(res.x, res.y, res.z);
	const EvaluatorCG eval{ chunk_grid, GridToSpaceConverter(res, min_pos, mesh_size, cell_size * 0.5f) };

	Vector3i grid_pos;
	const Vector3f hcs(cell_size * 0.5f);

	const Vector3i sub_box_end = sub_box.pos + sub_box.size;

	for (grid_pos.z = sub_box.pos.z; grid_pos.z < sub_box_end.z; ++grid_pos.z) {
		for (grid_pos.x = sub_box.pos.x; grid_pos.x < sub_box_end.x; ++grid_pos.x) {
			grid_pos.y = sub_box.pos.y;
			size_t grid_index = Vector3iUtil::get_zxy_index(grid_pos, res);

			for (; grid_pos.y < sub_box_end.y; ++grid_pos.y) {
				const float sd = eval(grid_pos);

				ZN_ASSERT(grid_index < sdf_grid.size());
				sdf_grid[grid_index] = sd;

				++grid_index;
			}
		}
	}
}

void generate_mesh_sdf_partitioned(Span<float> sdf_grid, const Vector3i res, Span<const Triangle> triangles,
		const Vector3f min_pos, const Vector3f max_pos, int subdiv) {
	// TODO Make this thread-local?
	ChunkGrid chunk_grid;
	partition_triangles(subdiv, triangles, min_pos, max_pos, chunk_grid);
	generate_mesh_sdf_partitioned(sdf_grid, res, Box3i(Vector3i(), res), min_pos, max_pos, chunk_grid);
}

CheckResult check_sdf(
		Span<const float> sdf_grid, Vector3i res, Span<const Triangle> triangles, Vector3f min_pos, Vector3f max_pos) {
	CheckResult result;
	result.ok = false;

	ZN_ASSERT_RETURN_V(math::is_valid_size(res), result);

	if (res.x == 0 || res.y == 0 || res.z == 0) {
		// Empty or incomparable, but ok
		result.ok = true;
		return result;
	}

	const float tolerance = 1.1f;
	const float max_variation = tolerance * get_max_sdf_variation(min_pos, max_pos, res);

	FixedArray<Vector3i, 6> dirs;
	dirs[0] = Vector3i(-1, 0, 0);
	dirs[1] = Vector3i(1, 0, 0);
	dirs[2] = Vector3i(0, -1, 0);
	dirs[3] = Vector3i(0, 1, 0);
	dirs[4] = Vector3i(0, 0, -1);
	dirs[5] = Vector3i(0, 0, 1);

	const Vector3f mesh_size = max_pos - min_pos;
	const Vector3f cell_size = mesh_size / Vector3f(res.x, res.y, res.z);
	const Evaluator eval{ triangles, GridToSpaceConverter(res, min_pos, mesh_size, cell_size * 0.5f) };

	Vector3i pos;
	for (pos.z = 0; pos.z < res.z; ++pos.z) {
		for (pos.x = 0; pos.x < res.x; ++pos.x) {
			for (pos.y = 0; pos.y < res.y; ++pos.y) {
				//
				for (unsigned int dir = 0; dir < dirs.size(); ++dir) {
					const Vector3i npos = pos + dirs[dir];

					if (npos.x < 0 || npos.y < 0 || npos.z < 0 || npos.x >= res.x || npos.y >= res.y ||
							npos.z >= res.z) {
						continue;
					}

					const unsigned int loc = Vector3iUtil::get_zxy_index(pos, res);
					const unsigned int nloc = Vector3iUtil::get_zxy_index(npos, res);

					const float v = sdf_grid[loc];
					const float nv = sdf_grid[nloc];

					const float variation = Math::abs(v - nv);

					if (variation > max_variation) {
						ZN_PRINT_VERBOSE(format("Found variation of {} > {}, {} and {}, at cell {} and {}", variation,
								max_variation, v, nv, pos, npos));

						const Vector3f posf0 = eval.grid_to_space(pos);
						const Vector3f posf1 = eval.grid_to_space(npos);

						result.ok = false;

						result.cell0.grid_pos = pos;
						result.cell0.mesh_pos = posf0;
						result.cell0.closest_triangle_index = get_closest_triangle_precalc(posf0, triangles);

						result.cell1.grid_pos = npos;
						result.cell1.mesh_pos = posf1;
						result.cell1.closest_triangle_index = get_closest_triangle_precalc(posf1, triangles);

						return result;
					}
				}
			}
		}
	}

	result.ok = true;
	return result;
}

void generate_mesh_sdf_naive(Span<float> sdf_grid, const Vector3i res, Span<const Triangle> triangles,
		const Vector3f min_pos, const Vector3f max_pos) {
	generate_mesh_sdf_naive(sdf_grid, res, Box3i(Vector3i(), res), triangles, min_pos, max_pos);
}

bool prepare_triangles(Span<const Vector3> vertices, Span<const int> indices, std::vector<Triangle> &triangles,
		Vector3f &out_min_pos, Vector3f &out_max_pos) {
	ZN_PROFILE_SCOPE();

	// The mesh can't be closed if it has less than 4 vertices
	ZN_ASSERT_RETURN_V(vertices.size() >= 4, false);

	// The mesh can't be closed if it has less than 4 triangles
	ZN_ASSERT_RETURN_V(indices.size() >= 12, false);
	ZN_ASSERT_RETURN_V(indices.size() % 3 == 0, false);

	triangles.resize(indices.size() / 3);

	for (size_t ti = 0; ti < triangles.size(); ++ti) {
		const int ii = ti * 3;
		const int i0 = indices[ii];
		const int i1 = indices[ii + 1];
		const int i2 = indices[ii + 2];

		Triangle &t = triangles[ti];
		t.v1 = to_vec3f(vertices[i0]);
		t.v2 = to_vec3f(vertices[i1]);
		t.v3 = to_vec3f(vertices[i2]);

		// Hack to make sure all points are distinct
		// const Vector3f midp = (t.v1 + t.v2 + t.v3) / 3.f;
		// const float shrink_amount = 0.0001f;
		// t.v1 = math::lerp(t.v1, midp, shrink_amount);
		// t.v2 = math::lerp(t.v2, midp, shrink_amount);
		// t.v3 = math::lerp(t.v3, midp, shrink_amount);
	}

	Vector3f min_pos = to_vec3f(vertices[0]);
	Vector3f max_pos = min_pos;

	for (int i = 0; i < vertices.size(); ++i) {
		const Vector3f p = to_vec3f(vertices[i]);
		min_pos = math::min(p, min_pos);
		max_pos = math::max(p, max_pos);
	}

	out_min_pos = min_pos;
	out_max_pos = max_pos;

	precalc_triangles(zylann::to_span(triangles));

	return true;
}

Vector3i auto_compute_grid_resolution(const Vector3f box_size, int cell_count) {
	const float cs = math::max(box_size.x, math::max(box_size.y, box_size.z)) / float(cell_count);
	return Vector3i(box_size.x / cs, box_size.y / cs, box_size.z / cs);
}

// Called from within the thread pool
void GenMeshSDFSubBoxTask::run(ThreadedTaskContext ctx) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT(shared_data != nullptr);

	VoxelBufferInternal &buffer = shared_data->buffer;
	const VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::CHANNEL_SDF;
	//ZN_ASSERT(!buffer.get_channel_compression(channel) == VoxelBufferInternal::COMPRESSION_NONE);
	Span<float> sdf_grid;
	ZN_ASSERT(buffer.get_channel_data(channel, sdf_grid));

	if (shared_data->use_chunk_grid) {
		generate_mesh_sdf_partitioned(
				sdf_grid, buffer.get_size(), box, shared_data->min_pos, shared_data->max_pos, shared_data->chunk_grid);
	} else {
		generate_mesh_sdf_naive(sdf_grid, buffer.get_size(), box, to_span(shared_data->triangles), shared_data->min_pos,
				shared_data->max_pos);
	}

	if (shared_data->pending_jobs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
		if (shared_data->boundary_sign_fix) {
			fix_sdf_sign_from_boundary(sdf_grid, buffer.get_size(), shared_data->min_pos, shared_data->max_pos);
		}
		// That was the last job
		on_complete();
	}
}

} // namespace zylann::voxel::mesh_sdf
