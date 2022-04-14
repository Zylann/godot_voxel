#include "funcs.h"
#include <core/math/vector3.h>

namespace zylann::math {

inline Vector3 fract(const Vector3 &p) {
	return Vector3(fract(p.x), fract(p.y), fract(p.z));
}

inline bool is_valid_size(const Vector3 &s) {
	return s.x >= 0 && s.y >= 0 && s.z >= 0;
}

inline bool has_nan(const Vector3 &v) {
	return Math::is_nan(v.x) || Math::is_nan(v.y) || Math::is_nan(v.z);
}

} // namespace zylann::math
