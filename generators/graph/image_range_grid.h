#ifndef IMAGE_RANGE_GRID_H
#define IMAGE_RANGE_GRID_H

#include "../../util/fixed_array.h"
#include "../../util/macros.h"
#include "../../util/math/interval.h"

ZN_GODOT_FORWARD_DECLARE(class Image)

namespace zylann {

// Stores minimum and maximum values over a 2D image at multiple levels of detail
class ImageRangeGrid {
public:
	~ImageRangeGrid();

	void clear();
	void generate(const Image &im);
	inline math::Interval get_range() const {
		return _total_range;
	}
	math::Interval get_range(math::Interval xr, math::Interval yr) const;

private:
	static const int MAX_LODS = 16;

	struct Lod {
		math::Interval *data = nullptr;
		int size_x = 0;
		int size_y = 0;
	};

	// Original size
	int _pixels_x = 0;
	int _pixels_y = 0;

	int _lod_base = 0;
	int _lod_count = 0;

	math::Interval _total_range;

	FixedArray<Lod, MAX_LODS> _lods;
};

} // namespace zylann

#endif // IMAGE_RANGE_GRID_H
