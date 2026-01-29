#include "aabb.h"
#include "../../io/text_writer.h"
#include "../../math/vector3.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const AABB &v) {
	w << "(o:";
	w << v.position;
	w << ", s:";
	w << v.size;
	w << ")";
	return w;
}

} // namespace zylann
