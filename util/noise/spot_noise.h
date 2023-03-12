#ifndef ZN_SPOT_NOISE_H
#define ZN_SPOT_NOISE_H

#include "../math/conv.h"
#include "../math/interval.h"

namespace zylann::SpotNoise {

// Very specialized kind of cellular noise for generating "spots" in a grid. Typical use case is ores in terrain.
// There are limitations, but they should not be noticeable for this use case.
// This implementation should be mostly self-contained and usable in GLSL too.

// Spot noise divides space into a grid, where each cell contains a "spot" at a random location. A distance is computed
// to return wether or not we are inside the spot. Unlike common cellular noise, "spot noise" does not lookup neighbor
// cells, so maximum jitter will cut-off the spots. However, the use case of ore generation makes the spots very sparse,
// so we can afford reducing jitter just enough. Not having to lookup neighbors makes the algorithm faster. There will
// be axis-aligned planes in which no spots can ever be found, but it's usually not an issue and can be masked with some
// coordinate displacement.

typedef Vector2i ivec2;
typedef Vector3i ivec3;
typedef Vector2f vec2;
typedef Vector3f vec3;

const int PRIME_X = 501125321;
const int PRIME_Y = 1136930381;
const int PRIME_Z = 1720413743;

// Derived from FastNoiseLite's cellular noise.
inline int hash2(Vector2i p, int seed) {
	int hash = seed ^ (p.x * PRIME_X) ^ (p.y * PRIME_Y);
	hash *= 0x27d4eb2d;
	return hash;
}

// Derived from FastNoiseLite's cellular noise.
inline int hash3(ivec3 p, int seed) {
	int hash = seed ^ (p.x * PRIME_X) ^ (p.y * PRIME_Y) ^ (p.z * PRIME_Z);
	hash *= 0x27d4eb2d;
	return hash;
}

// Standalone grid hash functions could be used in the future. Commenting for now cuz they are not used, yet

// inline float grid_hash_2d(vec2 pos, float cell_size, int seed) {
// 	ivec2 pi = to_vec2i(math::floor(pos / cell_size));
// 	int h = hash2(pi, seed);
// 	return float(h & 0xffff) / 65535.0;
// }

// inline float grid_hash_3d(vec3 pos, float cell_size, int seed) {
// 	ivec3 pi = to_vec3i(math::floor(pos / cell_size));
// 	int h = hash3(pi, seed);
// 	return float(h & 0xffff) / 65535.0;
// }

// inline vec2 grid_hash_3d_x2(vec3 pos, float cell_size, int seed) {
// 	ivec3 pi = to_vec3i(math::floor(pos / cell_size));
// 	int h = hash3(pi, seed);
// 	return to_vec2f(ivec2(h, h >> 16) & 0xffff) / 65535.0;
// }

inline vec2 hash_to_vec2(int h) {
	// 65536 possible locations along each axis
	return to_vec2f(ivec2(h, h >> 16) & 0xffff) / 65535.0;
}

inline vec3 hash_to_vec3(int h) {
	// 1024 possible locations along each axis
	return to_vec3f(ivec3(h, h >> 10, h >> 20) & 0x3ff) / 1024.0;
}

inline float spot_noise_2d(vec2 pos, float cell_size, float spot_size, float jitter, int seed) {
	vec2 cell_origin_norm = math::floor(pos / cell_size);
	ivec2 cell_origin_norm_i = to_vec2i(cell_origin_norm);
	int h = hash2(cell_origin_norm_i, seed);
	vec2 h2 = hash_to_vec2(h);
	vec2 spot_pos_norm = math::lerp(vec2(0.5), h2, jitter);
	float ds = math::distance_squared((cell_origin_norm + spot_pos_norm) * cell_size, pos);
	return float(ds < spot_size * spot_size);
}

inline float spot_noise_3d(vec3 pos, float cell_size, float spot_size, float jitter, int seed) {
	vec3 cell_origin_norm = math::floor(pos / cell_size);
	ivec3 cell_origin_norm_i = to_vec3i(cell_origin_norm);
	int h = hash3(cell_origin_norm_i, seed);
	vec3 h3 = hash_to_vec3(h);
	vec3 spot_pos_norm = math::lerp(vec3(0.5), h3, jitter);
	float ds = math::distance_squared((cell_origin_norm + spot_pos_norm) * cell_size, pos);
	return float(ds < spot_size * spot_size);
}

inline bool box_intersects(Vector2f a_min, Vector2f a_max, Vector2f b_min, Vector2f b_max) {
	if (a_min.x >= b_max.x) {
		return false;
	}
	if (a_min.y >= b_max.y) {
		return false;
	}
	if (b_min.x >= a_max.x) {
		return false;
	}
	if (b_min.y >= a_max.y) {
		return false;
	}
	return true;
}

inline bool box_intersects(Vector3f a_min, Vector3f a_max, Vector3f b_min, Vector3f b_max) {
	if (a_min.x >= b_max.x) {
		return false;
	}
	if (a_min.y >= b_max.y) {
		return false;
	}
	if (a_min.z >= b_max.z) {
		return false;
	}
	if (b_min.x >= a_max.x) {
		return false;
	}
	if (b_min.y >= a_max.y) {
		return false;
	}
	if (b_min.z >= a_max.z) {
		return false;
	}
	return true;
}

inline math::Interval spot_noise_2d_range(
		math::Interval2 pos, float cell_size, math::Interval spot_size, float jitter, int seed) {
	vec2 min_cell_origin_norm = math::floor(vec2(pos.x.min, pos.y.min) / cell_size);
	vec2 max_cell_origin_norm = math::floor(vec2(pos.x.max, pos.y.max) / cell_size);

	ivec2 min_cell_origin_norm_i = to_vec2i(min_cell_origin_norm);
	ivec2 max_cell_origin_norm_i = to_vec2i(max_cell_origin_norm);

	if (Vector2iUtil::get_area(max_cell_origin_norm_i - min_cell_origin_norm_i + ivec2(1, 1)) > 10) {
		// Don't bother checking too many cells, assume we'll intersect a spot.
		return math::Interval(0, 1);
	}

	vec2 box_size(pos.x.max - pos.x.min, pos.y.max - pos.y.min);
	if (math::min(box_size.x, box_size.y) >= 2.f * cell_size) {
		// We will intersect a spot.
		return math::Interval(0, 1);
	}

	// Check all cells intersecting with the area, and find if any spot intersects with it
	for (int yi = min_cell_origin_norm_i.y; yi <= max_cell_origin_norm_i.y; ++yi) {
		for (int xi = min_cell_origin_norm_i.x; xi <= max_cell_origin_norm_i.x; ++xi) {
			int h = hash2(ivec2(xi, yi), seed);
			vec2 h2 = hash_to_vec2(h);
			vec2 spot_pos_norm = math::lerp(vec2(0.5), h2, jitter);
			vec2 spot_pos = cell_size * (vec2(xi, yi) + spot_pos_norm);

			if (box_intersects(spot_pos - vec2(spot_size.max), spot_pos + vec2(spot_size.max),
						vec2(pos.x.min, pos.y.min), vec2(pos.x.max, pos.y.max))) {
				return math::Interval(0, 1);
			}
		}
	}

	return math::Interval::from_single_value(0);
}

inline math::Interval spot_noise_3d_range(
		math::Interval3 pos, float cell_size, math::Interval spot_size, float jitter, int seed) {
	vec3 min_cell_origin_norm = math::floor(vec3(pos.x.min, pos.y.min, pos.z.min) / cell_size);
	vec3 max_cell_origin_norm = math::floor(vec3(pos.x.max, pos.y.max, pos.z.max) / cell_size);

	ivec3 min_cell_origin_norm_i = to_vec3i(min_cell_origin_norm);
	ivec3 max_cell_origin_norm_i = to_vec3i(max_cell_origin_norm);

	if (Vector3iUtil::get_volume(max_cell_origin_norm_i - min_cell_origin_norm_i + ivec3(1, 1, 1)) > 30) {
		// Don't bother checking too many cells, assume we'll intersect a spot.
		return math::Interval(0, 1);
	}

	vec3 box_size(pos.x.max - pos.x.min, pos.y.max - pos.y.min, pos.z.max - pos.z.min);
	if (math::min(box_size.x, math::min(box_size.y, box_size.z)) >= 2.f * cell_size) {
		// We will intersect a spot.
		return math::Interval(0, 1);
	}

	// Check all cells intersecting with the area, and find if any spot intersects with it
	for (int zi = min_cell_origin_norm_i.z; zi <= max_cell_origin_norm_i.z; ++zi) {
		for (int yi = min_cell_origin_norm_i.y; yi <= max_cell_origin_norm_i.y; ++yi) {
			for (int xi = min_cell_origin_norm_i.x; xi <= max_cell_origin_norm_i.x; ++xi) {
				int h = hash3(ivec3(xi, yi, zi), seed);
				vec3 h3 = hash_to_vec3(h);
				vec3 spot_pos_norm = math::lerp(vec3(0.5), h3, jitter);
				vec3 spot_pos = cell_size * (vec3(xi, yi, zi) + spot_pos_norm);

				if (box_intersects(spot_pos - vec3(spot_size.max), spot_pos + vec3(spot_size.max),
							vec3(pos.x.min, pos.y.min, pos.z.min), vec3(pos.x.max, pos.y.max, pos.z.max))) {
					return math::Interval(0, 1);
				}
			}
		}
	}

	return math::Interval::from_single_value(0);
}

} // namespace zylann::SpotNoise

#endif // ZN_SPOT_NOISE_H
