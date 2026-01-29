#include "vector3.h"
#include "../io/text_writer.h"

namespace zylann {

TextWriter &operator<<(TextWriter &w, const Vector3 &v) {
	w << "(" << v.x << ", " << v.y << ", " << v.z << ")";
	return w;
}

} // namespace zylann
