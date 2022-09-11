#include "vector3i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Vector3i &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

} // namespace zylann
