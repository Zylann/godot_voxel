#ifndef ZN_SPOT_NOISE_GD_H
#define ZN_SPOT_NOISE_GD_H

#include "../godot/classes/noise.h"
#include "../godot/classes/resource.h"
#include "spot_noise.h"

namespace zylann {

class ZN_SpotNoise : public ZN_Noise {
	GDCLASS(ZN_SpotNoise, ZN_Noise);

public:
	int get_seed() const;
	void set_seed(int seed);

	float get_cell_size() const;
	void set_cell_size(float cell_size);

	float get_spot_radius() const;
	void set_spot_radius(float r);

	float get_jitter() const;
	void set_jitter(float jitter);

	inline float get_noise_2d_inline(const Vector2f pos) const {
		return SpotNoise::spot_noise_2d(pos, _cell_size, _spot_radius, _jitter, _seed);
	}

	inline float get_noise_3d_inline(const Vector3f pos) const {
		return SpotNoise::spot_noise_3d(pos, _cell_size, _spot_radius, _jitter, _seed);
	}

	PackedVector2Array get_spot_positions_in_area_2d(Rect2 rect) const;
	PackedVector3Array get_spot_positions_in_area_3d(AABB aabb) const;

protected:
	real_t _zn_get_noise_1d(real_t p_x) const override;
	real_t _zn_get_noise_2d(Vector2 p_v) const override;
	real_t _zn_get_noise_3d(Vector3 p_v) const override;

private:
	static void _bind_methods();

	int _seed = 1337;
	float _cell_size = 32.f;
	float _spot_radius = 3.f;
	float _jitter = 0.9f;
};

}; // namespace zylann

#endif // ZN_SPOT_NOISE_GD_H
