#include "vector3.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Vector3 &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

} // namespace zylann
