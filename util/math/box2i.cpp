#include "box2i.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Box2i &box) {
	w << "(o:";
	w << box.position;
	w << ", s:";
	w << box.size;
	w << ")";
	return w;
}

} // namespace zylann
