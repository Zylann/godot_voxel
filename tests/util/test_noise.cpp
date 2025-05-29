#include "test_noise.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite_range.h"
#include "../../util/testing/test_macros.h"

namespace zylann::tests {

void test_fnl_range() {
	Ref<ZN_FastNoiseLite> noise;
	noise.instantiate();
	noise->set_noise_type(ZN_FastNoiseLite::TYPE_OPEN_SIMPLEX_2S);
	noise->set_fractal_type(ZN_FastNoiseLite::FRACTAL_NONE);
	// noise->set_fractal_type(ZN_FastNoiseLite::FRACTAL_FBM);
	noise->set_fractal_octaves(1);
	noise->set_fractal_lacunarity(2.0);
	noise->set_fractal_gain(0.5);
	noise->set_period(512);
	noise->set_seed(0);

	const Vector3i min_pos(-1074, 1838, 5587);
	// const Vector3i max_pos(-1073, 1839, 5588);
	const Vector3i max_pos(-1058, 1854, 5603);

	const math::Interval x_range(min_pos.x, max_pos.x - 1);
	const math::Interval y_range(min_pos.y, max_pos.y - 1);
	const math::Interval z_range(min_pos.z, max_pos.z - 1);

	const math::Interval analytic_range = get_fnl_range_3d(**noise, x_range, y_range, z_range);

	math::Interval empiric_range;
	bool first_value = true;
	for (int z = min_pos.z; z < max_pos.z; ++z) {
		for (int y = min_pos.y; y < max_pos.y; ++y) {
			for (int x = min_pos.x; x < max_pos.x; ++x) {
				const float n = noise->get_noise_3d(x, y, z);
				if (first_value) {
					empiric_range.min = n;
					empiric_range.max = n;
					first_value = false;
				} else {
					empiric_range.min = math::min<real_t>(empiric_range.min, n);
					empiric_range.max = math::max<real_t>(empiric_range.max, n);
				}
			}
		}
	}

	ZN_TEST_ASSERT(analytic_range.contains(empiric_range));
}

} // namespace zylann::tests
