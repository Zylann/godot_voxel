#include "aabb.h"
#include "../../math/vector3.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const AABB &v) {
	ss << "(o:";
	ss << v.position;
	ss << ", s:";
	ss << v.size;
	ss << ")";
	return ss;
}

} // namespace zylann
