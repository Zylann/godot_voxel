#include "ids.h"
#include "../util/io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const SlotMapKey<uint16_t, uint16_t> &v) {
	w << "{i: " << v.index << ", v: " << v.version.value << "}";
	return w;
}

} // namespace zylann
