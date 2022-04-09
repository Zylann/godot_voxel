#include "vector3i.h"
#include <sstream>

std::stringstream &operator<<(std::stringstream &ss, const Vector3i &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}
