#include "image_range_grid.h"
#include "range_utility.h"

#include <core/image.h>

ImageRangeGrid::~ImageRangeGrid() {
	clear();
}

void ImageRangeGrid::clear() {
	for (int i = 0; i < _lod_count; ++i) {
		Lod &lod = _lods[i];
		if (lod.data != nullptr) {
			memdelete_arr(lod.data);
			lod.data = nullptr;
		}
	}
	_pixels_x = 0;
	_pixels_y = 0;
	_lod_base = 0;
	_lod_count = 0;
}

void ImageRangeGrid::generate(Image &im) {
	ERR_FAIL_COND_MSG(im.is_compressed(), String("Image format not supported: {0}").format(varray(im.get_format())));

	clear();

	const int lod_base = 4; // Start at 16

	// Compute first lod
	{
		const int chunk_size = 1 << lod_base;

		Lod &lod = _lods[0];
		lod.size_x = (im.get_width() - 1) / chunk_size + 1;
		lod.size_y = (im.get_height() - 1) / chunk_size + 1;
		lod.data = memnew_arr(Interval, lod.size_x * lod.size_y);

		for (int cy = 0; cy < lod.size_y; ++cy) {
			for (int cx = 0; cx < lod.size_x; ++cx) {
				const int min_x = cx * chunk_size;
				const int min_y = cy * chunk_size;
				const int max_x = min(min_x + chunk_size, im.get_width());
				const int max_y = min(min_y + chunk_size, im.get_height());

				const Interval r = get_heightmap_range(im, Rect2i(min_x, min_y, max_x - min_x, max_y - min_y));

				lod.data[cx + cy * lod.size_x] = r;
			}
		}
	}

	int lod_count = 1;

	// Compute next lods based on previous
	for (int lod_index = 1; lod_index < MAX_LODS; ++lod_index, ++lod_count) {
		const Lod &prev_lod = _lods[lod_index - 1];

		if (prev_lod.size_x == 1 && prev_lod.size_y == 1) {
			// Can't downscale further
			break;
		}

		Lod &lod = _lods[lod_index];
		CRASH_COND(lod.data != nullptr);

		lod.size_x = max(prev_lod.size_x >> 1, 1);
		lod.size_y = max(prev_lod.size_y >> 1, 1);

		lod.data = memnew_arr(Interval, lod.size_x * lod.size_y);

		int dst_i = 0;

		for (int cy = 0; cy < lod.size_y; ++cy) {
			const int src_y = min(cy * 2, prev_lod.size_y);

			for (int cx = 0; cx < lod.size_x; ++cx) {
				const int src_x = min(cx * 2, prev_lod.size_x);
				const int a = src_x + src_y * prev_lod.size_x;

				Interval r = prev_lod.data[a];

				if (src_x + 1 < prev_lod.size_x) {
					r.add_interval(prev_lod.data[a + 1]);

					if (src_y + 1 < prev_lod.size_y) {
						r.add_interval(prev_lod.data[a + prev_lod.size_x]);
						r.add_interval(prev_lod.data[a + prev_lod.size_x + 1]);
					}

				} else if (src_y + 1 < prev_lod.size_y) {
					r.add_interval(prev_lod.data[a + prev_lod.size_x]);
				}

				lod.data[dst_i++] = r;
			}
		}
	}

	{
		const int last_lod_index = lod_count - 1;
		CRASH_COND(last_lod_index < 0);
		const Lod &lod = _lods[last_lod_index];
		Interval r;
		for (int cy = 0; cy < lod.size_y; ++cy) {
			for (int cx = 0; cx < lod.size_x; ++cx) {
				r.add_interval(lod.data[cx + cy * lod.size_x]);
			}
		}
		_total_range = r;
	}

	_pixels_x = im.get_width();
	_pixels_y = im.get_height();
	_lod_base = lod_base;
	_lod_count = lod_count;
}

static void interval_to_pixels(Interval i, int &out_min, int &out_max, int len) {
	// Convert range to integer coordinates
	const int imin = static_cast<int>(Math::floor(i.min));
	const int imax = static_cast<int>(Math::ceil(i.max));

	// Images are finite, intervals are not.
	// It's useless to let the range span a potentially infinite area.
	// The image can repeat, so we clamp to one repetition.
	out_min = clamp(imin, -len, len);
	out_max = clamp(imax, -len, len);
}

Interval ImageRangeGrid::get_range(Interval xr, Interval yr) const {
	CRASH_COND(_lod_count <= 0);

	int pixel_min_x, pixel_max_x, pixel_min_y, pixel_max_y;
	interval_to_pixels(xr, pixel_min_x, pixel_max_x, _pixels_x);
	interval_to_pixels(yr, pixel_min_y, pixel_max_y, _pixels_y);

	// Find best LOD to use.
	// Depending on the length of the largest range, we may evaluate a different LOD to save iterations
	int pixel_len = max(pixel_max_x - pixel_min_x, pixel_max_y - pixel_min_y);
	const int cs = 1 << _lod_base;
	const int max_overlapping_chunks = 2;
	int lod_index = 0; // relative to _lod_base
	while (pixel_len > cs * max_overlapping_chunks && lod_index + 1 < _lod_count) {
		++lod_index;
		pixel_len >>= 1;
	}

	CRASH_COND(lod_index >= _lod_count);

	const int absolute_lod = _lod_base + lod_index;
	const int x_min = pixel_min_x >> absolute_lod;
	const int x_max = pixel_max_x >> absolute_lod;
	const int y_min = pixel_min_y >> absolute_lod;
	const int y_max = pixel_max_y >> absolute_lod;

	const Lod &lod = _lods[lod_index];

	// Accumulate overlapping chunks
	Interval r;
	for (int y = y_min; y <= y_max; ++y) {
		for (int x = x_min; x <= x_max; ++x) {
			r.add_interval(lod.data[(x % lod.size_x) + (y % lod.size_y) * lod.size_x]);
		}
	}

	return r;
}
