#include "vector2f.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Vector2f &v) {
	ss << "(" << v.x << ", " << v.y << ")";
	return ss;
}

} // namespace zylann
