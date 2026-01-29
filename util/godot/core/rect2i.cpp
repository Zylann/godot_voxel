#include "rect2i.h"
#include "../../io/text_writer.h"
#include "../../math/vector2i.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Rect2i &rect) {
	w << "(o:";
	w << rect.position;
	w << ", s:";
	w << rect.size;
	w << ")";
	return w;
}

} // namespace zylann
