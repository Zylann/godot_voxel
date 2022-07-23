#include "vector3.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Vector3 &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

} //namespace zylann
