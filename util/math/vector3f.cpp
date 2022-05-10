#include "vector3f.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Vector3f &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

} // namespace zylann
