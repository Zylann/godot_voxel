#include "image_utility.h"
#include "../../util/godot/classes/image.h"
#include "../../util/math/vector2i.h"
#include "../../util/string/format.h"

namespace zylann {

using namespace math;

Interval get_heightmap_range(const Image &im) {
	return get_heightmap_range(im, Rect2i(0, 0, im.get_width(), im.get_height()));
}

Interval get_heightmap_range(const Image &im, Rect2i rect) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V_MSG(!im.is_compressed(), Interval(), format("Image format not supported: {}", im.get_format()));
	ZN_ASSERT_RETURN_V_MSG(
			Rect2i(0, 0, im.get_width(), im.get_height()).encloses(rect),
			Interval(0, 0),
			format("Rectangle out of range: image size is {}, rectangle is {}", im.get_size(), rect)
	);
#endif

	Interval r;

	r.min = im.get_pixel(rect.position.x, rect.position.y).r;
	r.max = r.min;

	const int max_x = rect.position.x + rect.size.x;
	const int max_y = rect.position.y + rect.size.y;

	for (int y = rect.position.y; y < max_y; ++y) {
		for (int x = rect.position.x; x < max_x; ++x) {
			r.add_point(im.get_pixel(x, y).r);
		}
	}

	return r;
}

} // namespace zylann
