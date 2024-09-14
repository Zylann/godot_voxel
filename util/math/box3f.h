#ifndef ZN_BOX3F_H
#define ZN_BOX3F_H

#include "vector3t.h"
#include <type_traits>

namespace zylann {

// Axis-aligned 3D box using floating point coordinates.
template <typename T>
class Box3fT {
public:
	static_assert(std::is_floating_point<T>::value);

	Vector3T<T> min;
	Vector3T<T> max;

	static Box3fT<T> from_center_half_size(const Vector3T<T> center, const Vector3T<T> hs) {
		return { center - hs, center + hs };
	}

	bool contains(const Vector3T<T> &p_point) const {
		if (p_point.x < min.x) {
			return false;
		}
		if (p_point.y < min.y) {
			return false;
		}
		if (p_point.z < min.z) {
			return false;
		}
		if (p_point.x > max.x) {
			return false;
		}
		if (p_point.y > max.y) {
			return false;
		}
		if (p_point.z > max.z) {
			return false;
		}

		return true;
	}
};

using Box3f = Box3fT<float>;

} // namespace zylann

#endif // ZN_BOX3F_H
