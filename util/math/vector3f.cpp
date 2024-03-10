#include "vector3f.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Vector3f &v) {
	ss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return ss;
}

} // namespace zylann
