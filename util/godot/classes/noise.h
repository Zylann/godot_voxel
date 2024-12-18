#ifndef ZN_GODOT_NOISE_H
#define ZN_GODOT_NOISE_H

#include "../core/version.h"

#if defined(ZN_GODOT)
#include <modules/noise/noise.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/noise.hpp>
using namespace godot;
#endif

namespace zylann {

#if defined(ZN_GODOT) || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
class ZN_Noise : public Noise {
	GDCLASS(ZN_Noise, Noise)
#else
// Noise is abstract in the extension API and cannot be inherited
class ZN_Noise : public Resource {
	GDCLASS(ZN_Noise, Resource)
#endif
public:
#if defined(ZN_GODOT)
	real_t get_noise_1d(real_t p_x) const override;
	real_t get_noise_2dv(Vector2 p_v) const override;
	real_t get_noise_2d(real_t p_x, real_t p_y) const override;
	real_t get_noise_3dv(Vector3 p_v) const override;
	real_t get_noise_3d(real_t p_x, real_t p_y, real_t p_z) const override;
#elif defined(ZN_GODOT_EXTENSION) && (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
	real_t _get_noise_1d(real_t p_x) const override;
	real_t _get_noise_2d(Vector2 p_v) const override;
	real_t _get_noise_3d(Vector3 p_v) const override;
#endif

protected:
	virtual real_t _zn_get_noise_1d(real_t p_x) const;
	virtual real_t _zn_get_noise_2d(Vector2 p_v) const;
	virtual real_t _zn_get_noise_3d(Vector3 p_v) const;

	// #if defined(ZN_GODOT_EXTENSION) && (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 3)
	// 	void real_t _b_get_noise_2dc(real_t p_x, real_t p_y) const;
	// 	void real_t _b_get_noise_3dc(real_t p_x, real_t p_y, real_t p_z) const;
	// #endif

private:
	static void _bind_methods();
};

} // namespace zylann

#endif // ZN_GODOT_NOISE_H
