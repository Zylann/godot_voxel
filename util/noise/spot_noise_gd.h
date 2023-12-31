#ifndef ZN_SPOT_NOISE_GD_H
#define ZN_SPOT_NOISE_GD_H

#include "../godot/classes/resource.h"

namespace zylann {

class ZN_SpotNoise : public Resource {
	GDCLASS(ZN_SpotNoise, Resource);

public:
	int get_seed() const;
	void set_seed(int seed);

	float get_cell_size() const;
	void set_cell_size(float cell_size);

	float get_spot_radius() const;
	void set_spot_radius(float r);

	float get_jitter() const;
	void set_jitter(float jitter);

	float get_noise_2d(real_t x, real_t y) const;
	float get_noise_3d(real_t x, real_t y, real_t z) const;

	float get_noise_2dv(Vector2 pos) const;
	float get_noise_3dv(Vector3 pos) const;

	PackedVector2Array get_spot_positions_in_area_2d(Rect2 rect) const;
	PackedVector3Array get_spot_positions_in_area_3d(AABB aabb) const;

private:
	static void _bind_methods();

	int _seed = 1337;
	float _cell_size = 32.f;
	float _spot_radius = 3.f;
	float _jitter = 0.9f;
};

}; // namespace zylann

#endif // ZN_SPOT_NOISE_GD_H
