#include "interval.h"
#include <sstream>

namespace zylann {

std::stringstream &operator<<(std::stringstream &ss, const math::Interval &v) {
	ss << "[" << v.min << ", " << v.max << "]";
	return ss;
}

} // namespace zylann
