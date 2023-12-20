#include "box2i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Box2i &box) {
	ss << "(o:";
	ss << box.pos;
	ss << ", s:";
	ss << box.size;
	ss << ")";
	return ss;
}

} // namespace zylann
