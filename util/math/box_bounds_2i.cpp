#include "box_bounds_2i.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const BoxBounds2i &box) {
	ss << "(min:";
	ss << box.min_pos;
	ss << ", max:";
	ss << box.max_pos;
	ss << ")";
	return ss;
}

} // namespace zylann
