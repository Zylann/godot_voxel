#ifndef HEIGHTMAP_SDF_H
#define HEIGHTMAP_SDF_H

#include "array_slice.h"
#include <core/math/vector2.h>
#include <vector>

// Utility class to sample a heightmap as a 3D distance field.
// Provides a stateless function, or an accelerated area method using a cache.
// Note: this isn't general-purpose, it has been made for several use cases found in this module.
class HeightmapSdf {
public:
	enum Mode {
		SDF_VERTICAL = 0, // Lowest quality, fastest
		SDF_VERTICAL_AVERAGE,
		SDF_SEGMENT,
		SDF_MODE_COUNT
	};

	static const char *MODE_HINT_STRING;

	struct Range {
		float base = -50;
		float span = 200;

		inline float xform(float x) const {
			return x * span + base;
		}
	};

	struct Settings {
		Mode mode = SDF_VERTICAL;
		Range range;
	};

	struct Cache {
		std::vector<float> heights;
		int size_z = 0;

		inline float get_local(int x, int z) const {
			const int i = x + z * size_z;
#ifdef TOOLS_ENABLED
			CRASH_COND(i >= heights.size());
#endif
			return heights[i];
		}
	};

	Settings settings;

	// Precomputes data to accelerate the next area fetch.
	// ox, oz and stride are in world space.
	// Coordinates sent to the height function are in world space.
	template <typename Height_F>
	void build_cache(Height_F height_func, int cache_size_x, int cache_size_z, int ox, int oz, int stride) {

		CRASH_COND(cache_size_x < 0);
		CRASH_COND(cache_size_z < 0);

		if (settings.mode == SDF_SEGMENT) {
			// Pad
			cache_size_x += 2;
			cache_size_z += 2;
			ox -= stride;
			oz -= stride;
		}

		unsigned int area = cache_size_x * cache_size_z;
		if (area != _cache.heights.size()) {
			_cache.heights.resize(area);
		}
		_cache.size_z = cache_size_z;

		int i = 0;
		int gz = oz;

		for (int z = 0; z < cache_size_z; ++z, gz += stride) {
			int gx = ox;

			for (int x = 0; x < cache_size_x; ++x, gx += stride) {

				switch (settings.mode) {

					case SDF_VERTICAL:
					case SDF_SEGMENT:
						_cache.heights[i++] = settings.range.xform(height_func(gx, gz));
						break;

					case SDF_VERTICAL_AVERAGE:
						_cache.heights[i++] = settings.range.xform(get_height_blurred(height_func, gx, gz));
						break;

					default:
						CRASH_NOW();
						break;
				}
			}
		}
	}

	void clear_cache() {
		_cache.heights.clear();
	}

	// Core functionality is here.
	// Slower than using a cache, but doesn't rely on heap memory.
	// fx and fz use the same coordinate space as the height function.
	// gy0 and stride are world space.
	// Coordinates sent to the output function are in grid space.
	template <typename Height_F, typename Output_F>
	static void get_column_stateless(Output_F output_func, Height_F height_func, Mode mode, int fx, int gy0, int fz, int stride, int size_y) {

		switch (mode) {

			case SDF_VERTICAL: {
				float h = get_height_blurred(height_func, fx, fz);
				int gy = gy0;
				for (int y = 0; y < size_y; ++y, gy += stride) {
					float sdf = gy - h;
					output_func(y, sdf);
				}
			}

			case SDF_VERTICAL_AVERAGE: {
				float h = height_func(fx, fz);
				int gy = gy0;
				for (int y = 0; y < size_y; ++y, gy += stride) {
					float sdf = gy - h;
					output_func(y, sdf);
				}
			} break;

			case SDF_SEGMENT: {
				// Calculate distance to 8 segments going from the point at XZ to its neighbor points,
				// and pick the smallest distance.
				// Note: stride is intentionally not used for neighbor sampling.
				// More than 1 isn't really supported, because it causes inconsistencies when nearest-neighbor LOD is used.

				float h0 = height_func(fx - 1, fz - 1);
				float h1 = height_func(fx, fz - 1);
				float h2 = height_func(fx + 1, fz - 1);

				float h3 = height_func(fx - 1, fz);
				float h4 = height_func(fx, fz);
				float h5 = height_func(fx + 1, fz);

				float h6 = height_func(fx - 1, fz + 1);
				float h7 = height_func(fx, fz + 1);
				float h8 = height_func(fx + 1, fz + 1);

				const float sqrt2 = 1.414213562373095;

				int gy = gy0;
				for (int y = 0; y < size_y; ++y, gy += stride) {

					float sdf0 = get_constrained_segment_sdf(gy, h4, h0, sqrt2);
					float sdf1 = get_constrained_segment_sdf(gy, h4, h1, 1);
					float sdf2 = get_constrained_segment_sdf(gy, h4, h2, sqrt2);

					float sdf3 = get_constrained_segment_sdf(gy, h4, h3, 1);
					float sdf4 = gy - h4;
					float sdf5 = get_constrained_segment_sdf(gy, h4, h5, 1);

					float sdf6 = get_constrained_segment_sdf(gy, h4, h6, sqrt2);
					float sdf7 = get_constrained_segment_sdf(gy, h4, h7, 1);
					float sdf8 = get_constrained_segment_sdf(gy, h4, h8, sqrt2);

					float sdf = sdf4;

					if (Math::absf(sdf0) < Math::absf(sdf)) {
						sdf = sdf0;
					}
					if (Math::absf(sdf1) < Math::absf(sdf)) {
						sdf = sdf1;
					}
					if (Math::absf(sdf2) < Math::absf(sdf)) {
						sdf = sdf2;
					}
					if (Math::absf(sdf3) < Math::absf(sdf)) {
						sdf = sdf3;
					}
					if (Math::absf(sdf5) < Math::absf(sdf)) {
						sdf = sdf5;
					}
					if (Math::absf(sdf6) < Math::absf(sdf)) {
						sdf = sdf6;
					}
					if (Math::absf(sdf7) < Math::absf(sdf)) {
						sdf = sdf7;
					}
					if (Math::absf(sdf8) < Math::absf(sdf)) {
						sdf = sdf8;
					}

					output_func(y, sdf);
				}
			} break;

			default:
				CRASH_NOW();
				break;

		} // sdf mode
	}

	// Fastest if a cache has been built before. Prefer this when fetching areas.
	// Coordinates sent to the output function are in grid space.
	template <typename Output_F>
	inline void get_column_from_cache(Output_F output_func, int grid_x, int world_y0, int grid_z, int grid_size_y, int stride) {

		Mode mode = settings.mode;

		if (mode == SDF_VERTICAL_AVERAGE) {
			// Precomputed in cache, sample directly
			mode = SDF_VERTICAL;

		} else if (mode == SDF_SEGMENT) {
			// Pad
			++grid_x;
			++grid_z;
		}

		get_column_stateless(output_func,
				[&](int x, int z) { return _cache.get_local(x, z); },
				settings.mode, grid_x, world_y0, grid_z, stride, grid_size_y);
	}

private:
	static float get_constrained_segment_sdf(float p_yp, float p_ya, float p_yb, float p_xb);

	template <typename Height_F>
	static inline float get_height_blurred(Height_F height_func, int x, int y) {
		float h = height_func(x, y);
		h += height_func(x + 1, y);
		h += height_func(x - 1, y);
		h += height_func(x, y + 1);
		h += height_func(x, y - 1);
		return h * 0.2f;
	}

	Cache _cache;
};

#endif // HEIGHTMAP_SDF_H
