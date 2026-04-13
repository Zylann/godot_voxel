#include "box_bounds_2i.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const BoxBounds2i &box) {
	w << "(min:";
	w << box.min_pos;
	w << ", max:";
	w << box.max_pos;
	w << ")";
	return w;
}

} // namespace zylann
