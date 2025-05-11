#include "image_utility.h"
#include "../errors.h"
#include "../math/vector2i.h"
#include "../profiling.h"
#include "../string/format.h"
#include "core/packed_arrays.h"
#include "core/rect2i.h"
#include <limits>

namespace zylann::godot {
namespace ImageUtility {

bool check_args(const Image &im, Rect2i rect) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V_MSG(!im.is_compressed(), false, format("Image format not supported: {}", im.get_format()));
	ZN_ASSERT_RETURN_V_MSG(
			Rect2i(0, 0, im.get_width(), im.get_height()).encloses(rect),
			false,
			format("Rectangle out of range: image size is {}, rectangle is {}", im.get_size(), rect)
	);
#endif
	return true;
}

std::array<math::Interval, 4> get_range(const Image &im) {
	return get_range(im, Rect2i(Vector2i(), im.get_size()));
}

inline float u8_to_unorm(const uint8_t i) {
	return static_cast<float>(i) * (1.f / 255.f);
}

std::array<math::Interval, 4> get_range(const Image &im, const Rect2i rect) {
	ZN_PROFILE_SCOPE();

	std::array<math::Interval, 4> channel_ranges{};

	if (!check_args(im, rect)) {
		return channel_ranges;
	}

	const Vector2i maxp = rect.position + rect.size;

	const Image::Format format = im.get_format();
	const Vector2i im_size = im.get_size();

	switch (format) {
			// Fast-paths

		case Image::FORMAT_R8:
		case Image::FORMAT_L8: {
			const PackedByteArray pba = im.get_data();
			Span<const uint8_t> data = to_span(pba);
			uint8_t r_min = std::numeric_limits<uint8_t>::max();
			uint8_t r_max = std::numeric_limits<uint8_t>::min();
			for (int y = rect.position.y; y < maxp.y; ++y) {
				int i = rect.position.x + y * im_size.x;
				for (int x = rect.position.x; x < maxp.x; ++x, ++i) {
					const uint8_t c = data[i];
					r_min = math::min(r_min, c);
					r_max = math::max(r_max, c);
				}
			}
			channel_ranges[0] = math::Interval(u8_to_unorm(r_min), u8_to_unorm(r_max));
		} break;

		case Image::FORMAT_RG8: {
			struct Pixel {
				uint8_t r;
				uint8_t g;
			};
			const PackedByteArray pba = im.get_data();
			Span<const Pixel> data = to_span(pba).reinterpret_cast_to<const Pixel>();
			uint8_t r_min = std::numeric_limits<uint8_t>::max();
			uint8_t r_max = std::numeric_limits<uint8_t>::min();
			uint8_t g_min = std::numeric_limits<uint8_t>::max();
			uint8_t g_max = std::numeric_limits<uint8_t>::min();
			for (int y = rect.position.y; y < maxp.y; ++y) {
				int i = rect.position.x + y * im_size.x;
				for (int x = rect.position.x; x < maxp.x; ++x, ++i) {
					const Pixel c = data[i];
					r_min = math::min(r_min, c.r);
					r_max = math::max(r_max, c.r);
					g_min = math::min(g_min, c.g);
					g_max = math::max(g_max, c.g);
				}
			}
			channel_ranges[0] = math::Interval(u8_to_unorm(r_min), u8_to_unorm(r_max));
			channel_ranges[1] = math::Interval(u8_to_unorm(g_min), u8_to_unorm(g_max));
		} break;

		case Image::FORMAT_RF: {
			const PackedByteArray pba = im.get_data();
			Span<const float> data = to_span(pba).reinterpret_cast_to<const float>();
			float r_min = std::numeric_limits<float>::max();
			float r_max = std::numeric_limits<float>::min();
			for (int y = rect.position.y; y < maxp.y; ++y) {
				int i = rect.position.x + y * im_size.x;
				for (int x = rect.position.x; x < maxp.x; ++x, ++i) {
					const float c = data[i];
					r_min = math::min(r_min, c);
					r_max = math::max(r_max, c);
				}
			}
			channel_ranges[0] = math::Interval(r_min, r_max);
		} break;

		default: {
			// Slow generic

			{
				const Color c = im.get_pixel(rect.position.x, rect.position.y);
				for (unsigned int ci = 0; ci < channel_ranges.size(); ++ci) {
					channel_ranges[ci] = math::Interval::from_single_value(c[ci]);
				}
			}

			for (int y = rect.position.y; y < maxp.y; ++y) {
				for (int x = rect.position.x; x < maxp.x; ++x) {
					const Color c = im.get_pixel(x, y);
					for (unsigned int ci = 0; ci < channel_ranges.size(); ++ci) {
						channel_ranges[ci].add_point(c[ci]);
					}
				}
			}
		} break;
	}

	return channel_ranges;
}

} // namespace ImageUtility
} // namespace zylann::godot
