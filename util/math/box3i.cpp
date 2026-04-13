#include "box3i.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Box3i &box) {
	// TODO For some reason the one-liner version didn't compile?
	w << "(o:";
	w << box.position;
	w << ", s:";
	w << box.size;
	w << ")";
	return w;
}

} // namespace zylann
