#ifndef IMAGE_RANGE_GRID_H
#define IMAGE_RANGE_GRID_H

#include "../../util/fixed_array.h"
#include "../../util/math/interval.h"

class Image;

// Stores minimum and maximum values over a 2D image at multiple levels of detail
class ImageRangeGrid {
public:
	~ImageRangeGrid();

	void clear();
	void generate(Image &im);
	inline Interval get_range() const { return _total_range; }
	Interval get_range(Interval xr, Interval yr) const;

private:
	static const int MAX_LODS = 16;

	struct Lod {
		Interval *data = nullptr;
		int size_x = 0;
		int size_y = 0;
	};

	// Original size
	int _pixels_x = 0;
	int _pixels_y = 0;

	int _lod_base = 0;
	int _lod_count = 0;

	Interval _total_range;

	FixedArray<Lod, MAX_LODS> _lods;
};

#endif // IMAGE_RANGE_GRID_H
