#ifndef IMAGE_RANGE_GRID_H
#define IMAGE_RANGE_GRID_H

#include "../../util/containers/fixed_array.h"
#include "../../util/containers/std_vector.h"
#include "../../util/godot/macros.h"
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

	// Gets a quick upper bound of the range of values within an area of the image. If the area goes out of bounds of
	// the image, evaluation will be done as if the image repeats infinitely.
	math::Interval get_range_repeat(math::Interval xr, math::Interval yr) const;

private:
	static const int MAX_LODS = 16;

	struct Lod {
		// Grid of chunks containing the min and max of all pixels covered by each chunk
		StdVector<math::Interval> data;
		// In chunks
		int size_x = 0;
		int size_y = 0;
	};

	// Original size
	int _pixels_x = 0;
	int _pixels_y = 0;
	bool _pixels_x_is_power_of_2 = true;
	bool _pixels_y_is_power_of_2 = true;

	int _lod_base = 0;
	int _lod_count = 0;

	math::Interval _total_range;

	FixedArray<Lod, MAX_LODS> _lods;
};

} // namespace zylann

#endif // IMAGE_RANGE_GRID_H
