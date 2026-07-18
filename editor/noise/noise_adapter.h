#ifndef ZN_NOISE_ADAPTER_H
#define ZN_NOISE_ADAPTER_H

#ifdef VOXEL_ENABLE_FAST_NOISE_2
#include "../../util/noise/fast_noise_2.h"
#endif
#include "../../util/containers/span.h"
#include "../../util/noise/fast_noise_lite/fast_noise_lite.h"

namespace zylann {

// Godot still doesn't allow to inherit from `Noise` (and yet even wouldn't have methods we need) so we have to do this
// nonsense
// https://github.com/godotengine/godot/pull/100443
struct NoiseAdapter {
#ifdef VOXEL_ENABLE_FAST_NOISE_2
	Ref<FastNoise2> fn2;
#endif
	Ref<ZN_FastNoiseLite> fnl;

	void unset() {
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		fn2.unref();
#endif
		fnl.unref();
	}

	bool is_null() const {
		return fnl.is_null()
#ifdef VOXEL_ENABLE_FAST_NOISE_2
				&& fn2.is_null()
#endif
				;
	}

#ifdef VOXEL_ENABLE_FAST_NOISE_2
	void set(Ref<FastNoise2> noise) {
		unset();
		fn2 = noise;
	}
#endif

	void set(Ref<ZN_FastNoiseLite> noise) {
		unset();
		fnl = noise;
	}

	float get_noise_2d(const Vector2 pos) const {
		if (fnl.is_valid()) {
			return fnl->get_noise_2d(pos.x, pos.y);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		} else if (fn2.is_valid()) {
			return fn2->get_noise_2d_single(pos);
#endif
		}
		return 0.f;
	}

	float get_noise_3d(const Vector3 pos) const {
		if (fnl.is_valid()) {
			return fnl->get_noise_3d(pos.x, pos.y, pos.z);
#ifdef VOXEL_ENABLE_FAST_NOISE_2
		} else if (fn2.is_valid()) {
			return fn2->get_noise_3d_single(pos);
#endif
		}
		return 0.f;
	}

	void get_noise_2d_series(Span<const float> x, Span<const float> y, Span<float> out) const;
	void get_noise_3d_series(Span<const float> x, Span<const float> y, Span<const float> z, Span<float> out) const;
};

} // namespace zylann

#endif // ZN_NOISE_ADAPTER_H
