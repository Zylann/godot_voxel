#include "ids.h"
#include <sstream>

namespace zylann {

StdStringStream &operator<<(StdStringStream &ss, const SlotMapKey<uint16_t, uint16_t> &v) {
	ss << "{i: " << v.index << ", v: " << v.version.value << "}";
	return ss;
}

} // namespace zylann
