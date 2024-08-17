#include "rect2i.h"
#include "vector2i.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Rect2i &rect) {
	ss << "(o:";
	ss << rect.position;
	ss << ", s:";
	ss << rect.size;
	ss << ")";
	return ss;
}

} // namespace zylann
