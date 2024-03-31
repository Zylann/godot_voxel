#include "image_range_grid.h"
#include "../../util/godot/classes/image.h"
#include "../../util/string/format.h"
#include "range_utility.h"

namespace zylann {

using namespace math;

ImageRangeGrid::~ImageRangeGrid() {
	clear();
}

void ImageRangeGrid::clear() {
	_pixels_x = 0;
	_pixels_y = 0;
	_lod_base = 0;
	_lod_count = 0;
}

void ImageRangeGrid::generate(const Image &im) {
	ZN_ASSERT_RETURN_MSG(!im.is_compressed(), format("Image format not supported: {}", im.get_format()));

	clear();

	const int lod_base = 4; // Start at 16

	// Compute first lod
	{
		const int chunk_size = 1 << lod_base;

		Lod &lod = _lods[0];
		lod.size_x = math::ceildiv(im.get_width(), chunk_size);
		lod.size_y = math::ceildiv(im.get_height(), chunk_size);
		lod.data.resize(lod.size_x * lod.size_y);
		lod.data.shrink_to_fit();

		for (int cy = 0; cy < lod.size_y; ++cy) {
			for (int cx = 0; cx < lod.size_x; ++cx) {
				const int min_x = cx * chunk_size;
				const int min_y = cy * chunk_size;
				const int max_x = min(min_x + chunk_size, im.get_width());
				const int max_y = min(min_y + chunk_size, im.get_height());

				const Interval r = zylann::get_heightmap_range(im, Rect2i(min_x, min_y, max_x - min_x, max_y - min_y));

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

		lod.size_x = max(ceildiv(prev_lod.size_x, 2), 1);
		lod.size_y = max(ceildiv(prev_lod.size_y, 2), 1);

		lod.data.resize(lod.size_x * lod.size_y);
		lod.data.shrink_to_fit();

		int dst_i = 0;

		for (int cy = 0; cy < lod.size_y; ++cy) {
			const int src_y = min(cy * 2, prev_lod.size_y);

			for (int cx = 0; cx < lod.size_x; ++cx) {
				const int src_x = min(cx * 2, prev_lod.size_x);
				// Index of the lowest chunk in the previous LOD within the 2x2 region covered by the current chunk
				const int src_i = src_x + src_y * prev_lod.size_x;

				// Add 2x2 chunks from previous LOD covered by the current chunk, if available

				// X, Y
				Interval r = prev_lod.data[src_i];

				// X+1, Y
				if (src_x + 1 < prev_lod.size_x) {
					r.add_interval(prev_lod.data[src_i + 1]);
				}
				// X, Y+1
				if (src_y + 1 < prev_lod.size_y) {
					r.add_interval(prev_lod.data[src_i + prev_lod.size_x]);
				}
				// X+1, Y+1
				if (src_x + 1 < prev_lod.size_x && src_y + 1 < prev_lod.size_y) {
					r.add_interval(prev_lod.data[src_i + prev_lod.size_x + 1]);
				}

				lod.data[dst_i++] = r;
			}
		}
	}

	{
		ZN_ASSERT(lod_count > 0);
		const int last_lod_index = lod_count - 1;
		const Lod &lod = _lods[last_lod_index];
		Interval r = lod.data[0];
		for (int cy = 0; cy < lod.size_y; ++cy) {
			for (int cx = 0; cx < lod.size_x; ++cx) {
				r.add_interval(lod.data[cx + cy * lod.size_x]);
			}
		}
		_total_range = r;
	}

	_pixels_x = im.get_width();
	_pixels_y = im.get_height();
	_pixels_x_is_power_of_2 = math::is_power_of_two(_pixels_x);
	_pixels_y_is_power_of_2 = math::is_power_of_two(_pixels_y);
	_lod_base = lod_base;
	_lod_count = lod_count;
}

namespace {

void interval_to_pixels_repeat(Interval i, int &out_min, int &out_max, int image_len) {
	// Convert range to a positive integer coordinate space,
	// where coordinates extend up to twice the length of the image.
	// The interval gets wrapped within it, assuming a repeating image.

	int imin = static_cast<int>(Math::floor(i.min));
	int imax = static_cast<int>(Math::ceil(i.max));

	const int interval_len = imax - imin;

	if (interval_len >= image_len) {
		// The interval covers the whole length
		out_min = 0;
		out_max = image_len;
		return;
	}

	imin = math::wrap(imin, image_len);
	imax = math::wrap(imax, image_len);

	if (imin > imax) {
		imax = imin + interval_len;
	}

	// Keep it positive
	if (imin < 0) {
		imin += image_len;
		imax += image_len;
	}

	out_min = imin;
	out_max = imax;
}

} // namespace

Interval ImageRangeGrid::get_range_repeat(Interval xr, Interval yr) const {
	ZN_ASSERT(_lod_count > 0);

	int pixel_min_x, pixel_max_x, pixel_min_y, pixel_max_y;
	interval_to_pixels_repeat(xr, pixel_min_x, pixel_max_x, _pixels_x);
	interval_to_pixels_repeat(yr, pixel_min_y, pixel_max_y, _pixels_y);

	// Find best LOD to use.
	// Depending on the length of the largest range, we may evaluate a different LOD to save iterations
	int lod_index = 0; // relative to _lod_base
	{
		int pixel_len = max(pixel_max_x - pixel_min_x, pixel_max_y - pixel_min_y);
		const int cs = 1 << _lod_base;
		const int max_overlapping_chunks = 2;
		while (pixel_len > cs * max_overlapping_chunks && lod_index + 1 < _lod_count) {
			++lod_index;
			pixel_len >>= 1;
		}
	}

	ZN_ASSERT(lod_index < _lod_count);

	// Calculate the area in chunks
	const int absolute_lod = _lod_base + lod_index;
	const int chunk_x_min = arithmetic_rshift(pixel_min_x, absolute_lod);
	const int chunk_y_min = arithmetic_rshift(pixel_min_y, absolute_lod);
	int chunk_x_max = arithmetic_rshift(pixel_max_x, absolute_lod);
	int chunk_y_max = arithmetic_rshift(pixel_max_y, absolute_lod);

	// If the image size is not a power of 2 and the interval crosses the repeating boundary of the image, we have to
	// lookup one more chunk away, because chunks tile based on a rounded-up size of the image, yet the last chunk is
	// virtually shorter so the next repetition of the image starts sooner.
	// Note that `pixel_min_x` can't be >= `pixels_x`, because as soon as it is we wrap it back to the beginning of the
	// image.
	if (!_pixels_x_is_power_of_2 && pixel_max_x >= _pixels_x) {
		++chunk_x_max;
	}
	if (!_pixels_y_is_power_of_2 && pixel_max_y >= _pixels_y) {
		++chunk_y_max;
	}

	const Lod &lod = _lods[lod_index];

	// Accumulate overlapping chunks
	Interval r;
	{
		const unsigned int loc = math::wrap(chunk_x_min, lod.size_x) + math::wrap(chunk_y_min, lod.size_y) * lod.size_x;
		r = lod.data[loc];
	}
	for (int chunk_y = chunk_y_min; chunk_y <= chunk_y_max; ++chunk_y) {
		for (int chunk_x = chunk_x_min; chunk_x <= chunk_x_max; ++chunk_x) {
			const unsigned int loc = math::wrap(chunk_x, lod.size_x) + math::wrap(chunk_y, lod.size_y) * lod.size_x;
			r.add_interval(lod.data[loc]);
		}
	}

	return r;
}

} // namespace zylann
