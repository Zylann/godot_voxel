#include "box3i.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const Box3i &box) {
	// TODO For some reason the one-liner version didn't compile?
	ss << "(o:";
	ss << box.position;
	ss << ", s:";
	ss << box.size;
	ss << ")";
	return ss;
}

} // namespace zylann
