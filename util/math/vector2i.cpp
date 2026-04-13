#include "vector2i.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Vector2i &v) {
	w << "(" << v.x << ", " << v.y << ")";
	return w;
}

} // namespace zylann
