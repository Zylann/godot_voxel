#include "box3i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const Box3i &box) {
	// TODO For some reason the one-liner version didn't compile?
	ss << "(o:";
	ss << box.pos;
	ss << ", s:";
	ss << box.size;
	ss << ")";
	return ss;
}

} // namespace zylann
