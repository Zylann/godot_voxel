#include "vector2i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Vector2i &v) {
	ss << "(" << v.x << ", " << v.y << ")";
	return ss;
}

} // namespace zylann
