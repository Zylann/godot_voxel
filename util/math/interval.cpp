#include "interval.h"
#include "../string_funcs.h"
#include <sstream>

namespace zylann {
namespace math {

void Interval::_check_range_once() {
	static bool once = false;
	if (min > max && once == false) {
		once = true;
		ZN_PRINT_ERROR(format("Interval constructed with invalid range: min={}, max={}", min, max));
	}
}

} // namespace math

std::stringstream &operator<<(std::stringstream &ss, const math::Interval &v) {
	ss << "[" << v.min << ", " << v.max << "]";
	return ss;
}

} // namespace zylann
