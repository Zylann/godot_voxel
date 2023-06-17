#include "box_bounds_3i.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const BoxBounds3i &box) {
	ss << "(min:";
	ss << box.min_pos;
	ss << ", max:";
	ss << box.max_pos;
	ss << ")";
	return ss;
}

} // namespace zylann
