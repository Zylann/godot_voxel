#include "vector2i.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Vector2i &v) {
	ss << "(" << v.x << ", " << v.y << ")";
	return ss;
}

} // namespace zylann
