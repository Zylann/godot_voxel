#include "box_bounds_3i.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const BoxBounds3i &box) {
	w << "(min:";
	w << box.min_pos;
	w << ", max:";
	w << box.max_pos;
	w << ")";
	return w;
}

} // namespace zylann
