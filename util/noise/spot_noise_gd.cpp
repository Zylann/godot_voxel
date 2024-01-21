#include "spot_noise_gd.h"
#include "../math/conv.h"
#include "spot_noise.h"

namespace zylann {

int ZN_SpotNoise::get_seed() const {
	return _seed;
}

void ZN_SpotNoise::set_seed(int seed) {
	if (seed == _seed) {
		return;
	}
	_seed = seed;
	emit_changed();
}

float ZN_SpotNoise::get_cell_size() const {
	return _cell_size;
}

void ZN_SpotNoise::set_cell_size(float cell_size) {
	cell_size = math::max(cell_size, 0.01f);
	if (cell_size == _cell_size) {
		return;
	}
	_cell_size = cell_size;
	emit_changed();
}

float ZN_SpotNoise::get_spot_radius() const {
	return _spot_radius;
}

void ZN_SpotNoise::set_spot_radius(float r) {
	r = math::max(r, 0.01f);
	if (r == _spot_radius) {
		return;
	}
	_spot_radius = r;
	emit_changed();
}

float ZN_SpotNoise::get_jitter() const {
	return _jitter;
}

void ZN_SpotNoise::set_jitter(float jitter) {
	jitter = math::clamp(jitter, 0.f, 1.f);
	if (jitter == _jitter) {
		return;
	}
	_jitter = jitter;
	emit_changed();
}

float ZN_SpotNoise::get_noise_2d(real_t x, real_t y) const {
	return SpotNoise::spot_noise_2d(Vector2f(x, y), _cell_size, _spot_radius, _jitter, _seed);
}

float ZN_SpotNoise::get_noise_3d(real_t x, real_t y, real_t z) const {
	return SpotNoise::spot_noise_3d(Vector3f(x, y, z), _cell_size, _spot_radius, _jitter, _seed);
}

float ZN_SpotNoise::get_noise_2dv(Vector2 pos) const {
	return SpotNoise::spot_noise_2d(Vector2f(pos.x, pos.y), _cell_size, _spot_radius, _jitter, _seed);
}

float ZN_SpotNoise::get_noise_3dv(Vector3 pos) const {
	return SpotNoise::spot_noise_3d(Vector3f(pos.x, pos.y, pos.z), _cell_size, _spot_radius, _jitter, _seed);
}

PackedVector2Array ZN_SpotNoise::get_spot_positions_in_area_2d(Rect2 rect) const {
	PackedVector2Array positions;
	const Rect2 norm_rect(rect.position / _cell_size, rect.size / _cell_size);
	const Vector2i cminp = to_vec2i(norm_rect.position.floor());
	const Vector2i cmaxp = to_vec2i(norm_rect.get_end().ceil());
	Vector2i cpos;
	for (cpos.y = cminp.y; cpos.y < cmaxp.y; ++cpos.y) {
		for (cpos.x = cminp.x; cpos.x < cmaxp.x; ++cpos.x) {
			const Vector2 spot_pos = to_vec2(SpotNoise::get_spot_position_2d_norm(cpos, _jitter, _seed));
			if (norm_rect.has_point(spot_pos)) {
				positions.append(spot_pos * _cell_size);
			}
		}
	}
	return positions;
}

PackedVector3Array ZN_SpotNoise::get_spot_positions_in_area_3d(AABB aabb) const {
	PackedVector3Array positions;
	const AABB norm_aabb(aabb.position / _cell_size, aabb.size / _cell_size);
	const Vector3i cminp = to_vec3i(norm_aabb.position.floor());
	const Vector3i cmaxp = to_vec3i(norm_aabb.get_end().ceil());
	Vector3i cpos;
	for (cpos.z = cminp.z; cpos.z < cmaxp.z; ++cpos.z) {
		for (cpos.y = cminp.y; cpos.y < cmaxp.y; ++cpos.y) {
			for (cpos.x = cminp.x; cpos.x < cmaxp.x; ++cpos.x) {
				const Vector3 spot_pos = to_vec3(SpotNoise::get_spot_position_3d_norm(cpos, _jitter, _seed));
				if (norm_aabb.has_point(spot_pos)) {
					positions.append(spot_pos * _cell_size);
				}
			}
		}
	}
	return positions;
}

void ZN_SpotNoise::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &ZN_SpotNoise::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &ZN_SpotNoise::get_seed);

	ClassDB::bind_method(D_METHOD("set_cell_size", "cell_size"), &ZN_SpotNoise::set_cell_size);
	ClassDB::bind_method(D_METHOD("get_cell_size"), &ZN_SpotNoise::get_cell_size);

	ClassDB::bind_method(D_METHOD("set_spot_radius", "radius"), &ZN_SpotNoise::set_spot_radius);
	ClassDB::bind_method(D_METHOD("get_spot_radius"), &ZN_SpotNoise::get_spot_radius);

	ClassDB::bind_method(D_METHOD("set_jitter", "jitter"), &ZN_SpotNoise::set_jitter);
	ClassDB::bind_method(D_METHOD("get_jitter"), &ZN_SpotNoise::get_jitter);

	ClassDB::bind_method(D_METHOD("get_noise_2d", "x", "y"), &ZN_SpotNoise::get_noise_2d);
	ClassDB::bind_method(D_METHOD("get_noise_3d", "x", "y", "z"), &ZN_SpotNoise::get_noise_3d);

	ClassDB::bind_method(D_METHOD("get_noise_2dv", "pos"), &ZN_SpotNoise::get_noise_2dv);
	ClassDB::bind_method(D_METHOD("get_noise_3dv", "pos"), &ZN_SpotNoise::get_noise_3dv);

	ClassDB::bind_method(
			D_METHOD("get_spot_positions_in_area_2d", "rect"), &ZN_SpotNoise::get_spot_positions_in_area_2d);
	ClassDB::bind_method(
			D_METHOD("get_spot_positions_in_area_3d", "aabb"), &ZN_SpotNoise::get_spot_positions_in_area_3d);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spot_radius"), "set_spot_radius", "get_spot_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cell_size"), "set_cell_size", "get_cell_size");
	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "jitter", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_jitter", "get_jitter");
}

} // namespace zylann
