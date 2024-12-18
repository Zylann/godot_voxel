#include "noise.h"
#include "../../io/log.h"

namespace zylann {

#if defined(ZN_GODOT)

real_t ZN_Noise::get_noise_1d(real_t p_x) const {
	return _zn_get_noise_1d(p_x);
}

real_t ZN_Noise::get_noise_2dv(Vector2 p_v) const {
	return _zn_get_noise_2d(p_v);
}

real_t ZN_Noise::get_noise_2d(real_t p_x, real_t p_y) const {
	return _zn_get_noise_2d(Vector2(p_x, p_y));
}

real_t ZN_Noise::get_noise_3dv(Vector3 p_v) const {
	return _zn_get_noise_3d(p_v);
}

real_t ZN_Noise::get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const {
	return _zn_get_noise_3d(Vector3(p_x, p_y, p_z));
}

#elif defined(ZN_GODOT_EXTENSION) && (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)

real_t ZN_Noise::_get_noise_1d(real_t p_x) const {
	return _zn_get_noise_1d(p_x);
}

real_t ZN_Noise::_get_noise_2d(Vector2 p_v) const {
	return _zn_get_noise_2d(p_v);
}

real_t ZN_Noise::_get_noise_3d(Vector3 p_v) const {
	return _zn_get_noise_3d(p_v);
}

#endif

real_t ZN_Noise::_zn_get_noise_1d(real_t p_x) const {
	// Should be implemented in derived classes
	ZN_PRINT_ERROR_ONCE("Not implemented");
	return 0.0;
}

real_t ZN_Noise::_zn_get_noise_2d(Vector2 p_v) const {
	// Should be implemented in derived classes
	ZN_PRINT_ERROR_ONCE("Not implemented");
	return 0.0;
}

real_t ZN_Noise::_zn_get_noise_3d(Vector3 p_v) const {
	// Should be implemented in derived classes
	ZN_PRINT_ERROR_ONCE("Not implemented");
	return 0.0;
}

// #if defined(ZN_GODOT_EXTENSION) && (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 3)
// void real_t ZN_Noise::_b_get_noise_2dv(real_t p_x, real_t p_y) const {
// 	return _zn_get_noise_2d(Vector2(p_x, p_y));
// }

// void real_t ZN_Noise::_b_get_noise_3dv(real_t p_x, real_t p_y, real_t p_z) const {
// 	return _zn_get_noise_3d(Vector3(p_x, p_y, p_z));
// }
// #endif

void ZN_Noise::_bind_methods() {
	// #if defined(ZN_GODOT_EXTENSION) && (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 3)
	// 	ClassDB::bind_method(D_METHOD("get_noise_2d", "x", "y"), &ZN_FastNoiseLite::_zn_get_noise_2dc);
	// 	ClassDB::bind_method(D_METHOD("get_noise_3d", "x", "y", "z"), &ZN_FastNoiseLite::_zn_get_noise_2dc);

	// 	ClassDB::bind_method(D_METHOD("get_noise_2dv", "position"), &ZN_FastNoiseLite::_zn_get_noise_2d);
	// 	ClassDB::bind_method(D_METHOD("get_noise_3dv", "position"), &ZN_FastNoiseLite::_zn_get_noise_3d);
	// #endif
}

} // namespace zylann
