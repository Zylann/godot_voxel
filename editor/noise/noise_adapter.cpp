#include "noise_adapter.h"
#include "../../util/errors.h"

namespace zylann {

void NoiseAdapter::get_noise_2d_series(Span<const float> x, Span<const float> y, Span<float> out) const {
	if (fnl.is_valid()) {
		ZN_ASSERT_RETURN(x.size() == y.size() && y.size() == out.size());
		for (unsigned int i = 0; i < x.size(); ++i) {
			out[i] = fnl->get_noise_2d(x[i], y[i]);
		}
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	} else if (fn2.is_valid()) {
		fn2->get_noise_2d_series(x, y, out);
#endif
	} else {
		out.fill(0.f);
	}
}

void NoiseAdapter::get_noise_3d_series(
		Span<const float> x,
		Span<const float> y,
		Span<const float> z,
		Span<float> out
) const {
	if (fnl.is_valid()) {
		ZN_ASSERT_RETURN(x.size() == y.size() && y.size() == z.size() && z.size() == out.size());
		for (unsigned int i = 0; i < x.size(); ++i) {
			out[i] = fnl->get_noise_3d(x[i], y[i], z[i]);
		}
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	} else if (fn2.is_valid()) {
		fn2->get_noise_3d_series(x, y, z, out);
#endif
	} else {
		out.fill(0.f);
	}
}

} // namespace zylann
