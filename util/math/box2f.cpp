#include "box2f.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Box2f &box) {
	w << "(min:";
	w << box.min;
	w << ", max:";
	w << box.max;
	w << ")";
	return w;
}

} // namespace zylann
