#include "box2f.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Box2f &box) {
	ss << "(min:";
	ss << box.min;
	ss << ", max:";
	ss << box.max;
	ss << ")";
	return ss;
}

} // namespace zylann
