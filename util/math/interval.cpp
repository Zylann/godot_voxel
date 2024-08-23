#include "interval.h"
#include "../string/format.h"
#include <sstream>

namespace zylann {

namespace math {
namespace interval_impl {

template <typename T>
inline void check_range_once_t(T min, T max) {
	static bool once = false;
	if (min > max && once == false) {
		once = true;
		ZN_PRINT_ERROR(format("Interval constructed with invalid range: min={}, max={}", min, max));
	}
}

void check_range_once(float min, float max) {
	check_range_once_t(min, max);
}

void check_range_once(double min, double max) {
	check_range_once_t(min, max);
}

} // namespace interval_impl
} // namespace math

StdStringStream &operator<<(StdStringStream &ss, const math::Interval &v) {
	ss << "[" << v.min << ", " << v.max << "]";
	return ss;
}

} // namespace zylann
