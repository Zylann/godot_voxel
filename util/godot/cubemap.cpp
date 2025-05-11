#include "cubemap.h"
#include "../containers/span.h"
#include "../math/box2f.h"
#include "../math/box_bounds_2i.h"
#include "../math/conv.h"
#include "../profiling.h"
#include "classes/cubemap.h"
#include "classes/image_texture_layered.h"
#include "core/packed_arrays.h"
#include "image_utility.h"
#include <array>
#include <cstdint>

namespace zylann {

void ZN_Cubemap::create(const unsigned int resolution, const Image::Format format) {
	ZN_ASSERT_RETURN_MSG(!zylann::godot::is_format_compressed(format), "Compressed formats are not supported");
	for (Ref<Image> &im_ref : _images) {
		im_ref = zylann::godot::create_empty_image(resolution, resolution, false, format);
	}
	_padded = false;
}

void ZN_Cubemap::create_from_images(TypedArray<Image> images) {
	ZN_PRINT_ERROR("Not implemented");
	// TODO
}

bool ZN_Cubemap::is_valid() const {
	return _images[0].is_valid();
}

unsigned int ZN_Cubemap::get_resolution() const {
	ZN_ASSERT(is_valid());
	return _images[0]->get_size().x;
}

Image::Format ZN_Cubemap::get_format() const {
	if (!is_valid()) {
		return Image::FORMAT_MAX;
	}
	return _images[0]->get_format();
}

Image &ZN_Cubemap::get_image(const unsigned int side) {
#ifdef DEV_ENABLED
	ZN_ASSERT(_images[side].is_valid());
#endif
	return **_images[side];
}

const Image &ZN_Cubemap::get_image(const unsigned int side) const {
#ifdef DEV_ENABLED
	ZN_ASSERT(_images[side].is_valid());
#endif
	return **_images[side];
}

const Ref<Image> ZN_Cubemap::get_image_ref(const unsigned int side) const {
	return _images[side];
}

Ref<Cubemap> ZN_Cubemap::create_texture() const {
	if (!is_valid()) {
		return Ref<Cubemap>();
	}

	TypedArray<Image> images;
	images.resize(_images.size());

	for (unsigned int side = 0; side < SIDE_COUNT; ++side) {
		Ref<Image> image = _images[side];
		if (_padded) {
			image = image->duplicate();
			image->crop_from_point(1, 1, image->get_width() - 2, image->get_height() - 2);
		}
		images[side] = image;
	}

	Ref<Cubemap> tex;
	tex.instantiate();
	zylann::godot::create_from_images(**tex, images);

	return tex;
}

//           o--------o
//           |        |
//           |   +Y   |
//           |        |
//  o--------o--------o--------o--------o
//  |        |        |        |        |
//  |   -X   |   +Z   |   +X   |   -Z   |        Y
//  |        |        |        |        |        |
//  o--------o--------o--------o--------o        o---X
//           |        |                         /
//           |   -Y   |                        Z
//           |        |
//           o--------o
//
// Convention taken from Godot, applying a cubemap shader to a cube using the Forward+ (Vulkan) renderer.

inline ZN_Cubemap::UV convert_xyz_to_cube_uv(const Vector3f p) {
	// https://www.gamedev.net/forums/topic/687535-implementing-a-cube-map-lookup-function/
	const Vector3f ap = math::abs(p);
	float ma;
	Vector2f uv;
	ZN_Cubemap::SideIndex f;

	if (ap.z >= ap.x && ap.z >= ap.y) {
		f = p.z < 0 ? ZN_Cubemap::SIDE_NEGATIVE_Z : ZN_Cubemap::SIDE_POSITIVE_Z;
		// Protect from NaN when sampling at (0,0,0)
		ma = 0.5f / math::max(ap.z, 0.0001f);
		uv = Vector2f(p.z < 0.f ? -p.x : p.x, -p.y);

	} else if (ap.y >= ap.x) {
		f = p.y < 0.f ? ZN_Cubemap::SIDE_NEGATIVE_Y : ZN_Cubemap::SIDE_POSITIVE_Y;
		ma = 0.5f / math::max(ap.y, 0.0001f);
		uv = Vector2f(p.x, p.y < 0.f ? -p.z : p.z);

	} else {
		f = p.x < 0.f ? ZN_Cubemap::SIDE_NEGATIVE_X : ZN_Cubemap::SIDE_POSITIVE_X;
		ma = 0.5f / math::max(ap.x, 0.0001f);
		uv = Vector2f(p.x < 0.f ? p.z : -p.z, -p.y);
	}

	return { f, uv * ma + Vector2f(0.5f) };
}

ZN_Cubemap::UV ZN_Cubemap::get_uv_from_xyz(const Vector3f pos) {
	return convert_xyz_to_cube_uv(pos);
}

Vector3f ZN_Cubemap::get_xyz_from_uv(const Vector2f uv, const SideIndex face) {
	switch (face) {
		case SIDE_POSITIVE_X:
			return get_xyz_from_uv<SIDE_POSITIVE_X>(uv);
		case SIDE_NEGATIVE_X:
			return get_xyz_from_uv<SIDE_NEGATIVE_X>(uv);
		case SIDE_POSITIVE_Y:
			return get_xyz_from_uv<SIDE_POSITIVE_Y>(uv);
		case SIDE_NEGATIVE_Y:
			return get_xyz_from_uv<SIDE_NEGATIVE_Y>(uv);
		case SIDE_POSITIVE_Z:
			return get_xyz_from_uv<SIDE_POSITIVE_Z>(uv);
		case SIDE_NEGATIVE_Z:
			return get_xyz_from_uv<SIDE_NEGATIVE_Z>(uv);
		default:
			ZN_PRINT_ERROR("Invalid face");
			return Vector3f();
	}
}

Color ZN_Cubemap::sample_nearest(const Vector3f position) const {
	const UV uv = get_uv_from_xyz(position);
	const Image &im = **_images[uv.side];
	const Vector2i im_size = im.get_size();
	const Vector2f ts = to_vec2f(im_size);
	const Vector2i uv_px = math::min(to_vec2i(uv.position * ts), im_size - Vector2i(1, 1));
	return im.get_pixelv(uv_px);
}

Color ZN_Cubemap::sample_nearest_prepad(const Vector3f position) const {
	const UV uv = get_uv_from_xyz(position);
	const Image &im = **_images[uv.side];
	const Vector2i im_size = im.get_size();
	const Vector2f ts = to_vec2f(im_size - Vector2i(2, 2));
	const Vector2i uv_px = math::min(to_vec2i(uv.position * ts) + Vector2i(1, 1), im_size - Vector2i(1, 1));
	return im.get_pixelv(uv_px);
}

enum ImageSide {
	IM_SIDE_NEGATIVE_X,
	IM_SIDE_POSITIVE_X,
	IM_SIDE_NEGATIVE_Y,
	IM_SIDE_POSITIVE_Y,
};

template <int SIDE, int PAD, bool FLIP>
inline Vector2i get_side_pos(int i, int res) {
	static_assert(SIDE >= 0 && SIDE < 4);
	if (FLIP) {
		i = res - i - 1;
	}
	switch (SIDE) {
		case IM_SIDE_NEGATIVE_X:
			return Vector2i(PAD, i);
		case IM_SIDE_POSITIVE_X:
			return Vector2i(res - 1 - PAD, i);
		case IM_SIDE_NEGATIVE_Y:
			return Vector2i(i, PAD);
		case IM_SIDE_POSITIVE_Y:
			return Vector2i(i, res - 1 - PAD);
		default:
			// TODO Unreachable
			return Vector2i();
	}
}

template <int SRC_SIDE, int DST_SIDE, bool SRC_FLIP>
void copy_side_pixels_pad(Image &dst, const Image &src) {
	ZN_ASSERT(dst.get_size() == src.get_size());
	const Vector2i res = dst.get_size();
	ZN_ASSERT(res.x == res.y);
	for (int i = 1; i < res.x - 1; ++i) {
		const Vector2i src_pos = get_side_pos<SRC_SIDE, 1, SRC_FLIP>(i, res.x);
		const Vector2i dst_pos = get_side_pos<DST_SIDE, 0, false>(i, res.x);
		const Color src_col = src.get_pixelv(src_pos);
		dst.set_pixelv(dst_pos, src_col);
	}
}

// Pads each image by 1 pixel containing copies of neighbor pixels for fast linear sampling
void ZN_Cubemap::make_linear_filterable() {
	ZN_PROFILE_SCOPE();

	if (_padded) {
		return;
	}
	_padded = true;

	std::array<Ref<Image>, SIDE_COUNT> &images = _images;

	{
		ZN_PROFILE_SCOPE_NAMED("Pad");
		for (Ref<Image> &im_ref : images) {
			const Vector2i res = im_ref->get_size();
			Ref<Image> imc = Image::create_empty(res.x + 2, res.y + 2, false, im_ref->get_format());
			imc->blit_rect(im_ref, Rect2i(Vector2i(), res), Vector2i(1, 1));
			im_ref = imc;
		}
	}

	{
		ZN_PROFILE_SCOPE_NAMED("Copy edges");

		Image &nx = **images[SIDE_NEGATIVE_X];
		Image &px = **images[SIDE_POSITIVE_X];
		Image &ny = **images[SIDE_NEGATIVE_Y];
		Image &py = **images[SIDE_POSITIVE_Y];
		Image &nz = **images[SIDE_NEGATIVE_Z];
		Image &pz = **images[SIDE_POSITIVE_Z];

		// -X
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_NEGATIVE_X, false>(nx, nz);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_POSITIVE_X, false>(nx, pz);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_NEGATIVE_Y, false>(nx, py);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_POSITIVE_Y, true>(nx, ny);
		// +X
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_NEGATIVE_X, false>(px, pz);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_POSITIVE_X, false>(px, nz);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_NEGATIVE_Y, true>(px, py);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_POSITIVE_Y, false>(px, ny);
		// -Y
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_NEGATIVE_X, true>(ny, nx);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_POSITIVE_X, false>(ny, px);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_NEGATIVE_Y, false>(ny, pz);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_POSITIVE_Y, true>(ny, nz);
		// +Y
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_NEGATIVE_X, false>(py, nx);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_POSITIVE_X, true>(py, px);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_NEGATIVE_Y, true>(py, nz);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_POSITIVE_Y, false>(py, pz);
		// -Z
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_NEGATIVE_X, false>(nz, px);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_POSITIVE_X, false>(nz, nx);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_NEGATIVE_Y, true>(nz, py);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_POSITIVE_Y, true>(nz, ny);
		// +Z
		copy_side_pixels_pad<IM_SIDE_POSITIVE_X, IM_SIDE_NEGATIVE_X, false>(pz, nx);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_X, IM_SIDE_POSITIVE_X, false>(pz, px);
		copy_side_pixels_pad<IM_SIDE_POSITIVE_Y, IM_SIDE_NEGATIVE_Y, false>(pz, py);
		copy_side_pixels_pad<IM_SIDE_NEGATIVE_Y, IM_SIDE_POSITIVE_Y, false>(pz, ny);
	}

	// Set corners to average of neighbor sides
	for (Ref<Image> &im_ref : images) {
		Image &im = **im_ref;
		const Vector2i res = im.get_size();
		{
			// Set to average of 3 pixels around the corner, so it will be the same for all 3 textures
			const Color c0 = im.get_pixelv(Vector2i(0, 1));
			const Color c1 = im.get_pixelv(Vector2i(1, 0));
			const Color c2 = im.get_pixelv(Vector2i(1, 1));
			const Color cc = (c0 + c1 + c2) / 3.f;
			im.set_pixelv(Vector2i(0, 0), cc);
		}
		{
			const Color c0 = im.get_pixelv(Vector2i(res.x - 2, 0));
			const Color c1 = im.get_pixelv(Vector2i(res.x - 1, 1));
			const Color c2 = im.get_pixelv(Vector2i(res.x - 2, 1));
			const Color cc = (c0 + c1 + c2) / 3.f;
			im.set_pixelv(Vector2i(res.x - 1, 0), cc);
		}
		{
			const Color c0 = im.get_pixelv(Vector2i(res.x - 1, res.y - 2));
			const Color c1 = im.get_pixelv(Vector2i(res.x - 2, res.y - 1));
			const Color c2 = im.get_pixelv(Vector2i(res.x - 2, res.y - 2));
			const Color cc = (c0 + c1 + c2) / 3.f;
			im.set_pixelv(Vector2i(res.x - 1, res.y - 1), cc);
		}
		{
			const Color c0 = im.get_pixelv(Vector2i(1, res.y - 1));
			const Color c1 = im.get_pixelv(Vector2i(0, res.y - 2));
			const Color c2 = im.get_pixelv(Vector2i(1, res.y - 2));
			const Color cc = (c0 + c1 + c2) / 3.f;
			im.set_pixelv(Vector2i(0, res.y - 1), cc);
		}
	}
}

static inline Color get_pixel_linear_unchecked(const Image &im, float x, float y, int im_w, int im_h) {
	const int x0 = static_cast<int>(math::floor(x));
	const int y0 = static_cast<int>(math::floor(y));
	const int x1 = math::min(x0 + 1, im_w - 1);
	const int y1 = math::min(y0 + 1, im_h - 1);

	const float xf = x - x0;
	const float yf = y - y0;

	const Color c00 = im.get_pixel(x0, y0);
	const Color c10 = im.get_pixel(x1, y0);
	const Color c01 = im.get_pixel(x0, y1);
	const Color c11 = im.get_pixel(x1, y1);

	const Color cl = math::lerp(math::lerp(c00, c10, xf), math::lerp(c01, c11, xf), yf);

	return cl;
}

Color ZN_Cubemap::sample_linear_prepad(const Vector3f position) const {
	// https://www.gamedev.net/forums/topic/572117-bilinear-sampling-of-cube-map-using-cpu/
	// const CubemapUV uv = convert_xyz_to_cube_uv(position);
	const UV uv = get_uv_from_xyz(position);
	const Image &im = **_images[uv.side];
	const Vector2i res = im.get_size();
	const Vector2f ts = to_vec2f(res - Vector2i(2, 2));
	const Vector2f uv_px = uv.position * ts;
	// We make it so linear filtering is centered on pixels to make it symetrical, otherwise sides of the cubemap
	// don't line up (haven't fully figured out why tho)
	return get_pixel_linear_unchecked(im, uv_px.x + 0.5f, uv_px.y + 0.5f, res.x, res.y);
}

namespace pixelconv {

template <typename T, unsigned int N>
struct Direct {
	using Src = std::array<T, N>;
	using Dst = std::array<T, N>;
	static inline Dst convert(const Src src) {
		return src;
	}
};

template <unsigned int N>
struct U8ToFloat {
	using Src = std::array<uint8_t, N>;
	using Dst = std::array<float, N>;
	static inline Dst convert(const Src src) {
		Dst dst;
		for (unsigned int ci = 0; ci < N; ++ci) {
			dst[ci] = static_cast<float>(src[ci]) * (1.f / 255.f);
		}
		return dst;
	}
};

} // namespace pixelconv

template <typename TConv>
static inline typename TConv::Dst get_pixel_linear_unchecked(
		const Span<const typename TConv::Src> im,
		const float x,
		const float y,
		const int im_w,
		const int im_h
) {
	const int x0 = static_cast<int>(math::floor(x));
	const int y0 = static_cast<int>(math::floor(y));
	const int x1 = math::min(x0 + 1, im_w - 1);
	const int y1 = math::min(y0 + 1, im_h - 1);

	const float xf = x - x0;
	const float yf = y - y0;

	const TConv::Dst c00 = TConv::convert(im[x0 + y0 * im_w]);
	const TConv::Dst c10 = TConv::convert(im[x1 + y0 * im_w]);
	const TConv::Dst c01 = TConv::convert(im[x0 + y1 * im_w]);
	const TConv::Dst c11 = TConv::convert(im[x1 + y1 * im_w]);

	// const TConv::Dst cl = math::lerp(math::lerp(c00, c10, xf), math::lerp(c01, c11, xf), yf);
	typename TConv::Dst cl;
	for (unsigned int ci = 0; ci < cl.size(); ++ci) {
		cl[ci] = math::lerp(math::lerp(c00[ci], c10[ci], xf), math::lerp(c01[ci], c11[ci], xf), yf);
	}

	return cl;
}

template <typename TConv>
void sample_linear_prepad_unchecked(
		Span<const float> x_array,
		Span<const float> y_array,
		Span<const float> z_array,
		std::array<Span<float>, 4> dst_channels,
		const std::array<Ref<Image>, ZN_Cubemap::SIDE_COUNT> &gd_ims
) {
	std::array<Span<const TConv::Src>, ZN_Cubemap::SIDE_COUNT> ims;

	for (unsigned int side = 0; side < ims.size(); ++side) {
		const Image &im = **gd_ims[side];
		const PackedByteArray pba = im.get_data();
		ims[side] = to_span(pba).reinterpret_cast_to<const TConv::Src>();
	}

	const Vector2i res = gd_ims[0]->get_size();
	const Vector2f ts = to_vec2f(res - Vector2i(2, 2));

	for (unsigned int i = 0; i < x_array.size(); ++i) {
		const Vector3f position(x_array[i], y_array[i], z_array[i]);
		// const ZN_Cubemap::UV uv = ZN_Cubemap::get_uv_from_xyz(position);
		// Use inline version. MUCH faster
		const ZN_Cubemap::UV uv = convert_xyz_to_cube_uv(position);

		const Span<const TConv::Src> im = ims[uv.side];
		const Vector2f uv_px = uv.position * ts;
		// We make it so linear filtering is centered on pixels to make it symetrical, otherwise sides of the cubemap
		// don't line up (haven't fully figured out why tho)
		const TConv::Dst c = get_pixel_linear_unchecked<TConv>(im, uv_px.x + 0.5f, uv_px.y + 0.5f, res.x, res.y);

		for (unsigned int ci = 0; ci < c.size(); ++ci) {
			if (dst_channels[ci].size() != 0) {
				dst_channels[ci][i] = c[ci];
			}
		}
	}

	typename TConv::Dst temp;
	for (unsigned int ci = temp.size(); ci < 4; ++ci) {
		if (dst_channels[ci].size() != 0) {
			dst_channels[ci].fill(0);
		}
	}
}

void ZN_Cubemap::sample_linear_prepad(
		Span<const float> x_array,
		Span<const float> y_array,
		Span<const float> z_array,
		Span<float> out_r,
		Span<float> out_g,
		Span<float> out_b,
		Span<float> out_a
) const {
	ZN_PROFILE_SCOPE();

	// TODO Could be optimized further if we have a hint about sampling only one side

	std::array<Span<float>, 4> dst_channels{ out_r, out_g, out_b, out_a };

#ifdef DEV_ENABLED
	ZN_ASSERT(is_valid());
	ZN_ASSERT(x_array.size() == y_array.size() && y_array.size() == z_array.size());

	for (unsigned int ci = 0; ci < dst_channels.size(); ++ci) {
		if (dst_channels[ci].size() != 0) {
			ZN_ASSERT(dst_channels[ci].size() == x_array.size());
		}
	}
#endif

	if (out_r.size() == 0 && out_g.size() == 0 && out_b.size() == 0 && out_a.size() == 0) {
		return;
	}
	if (x_array.size() == 0) {
		return;
	}

	const Image::Format format = get_format();

	switch (format) {
			// Fast-paths

		case Image::FORMAT_R8:
		case Image::FORMAT_L8: {
			sample_linear_prepad_unchecked<pixelconv::U8ToFloat<1>>(x_array, y_array, z_array, dst_channels, _images);
		} break;

		case Image::FORMAT_RG8: {
			sample_linear_prepad_unchecked<pixelconv::U8ToFloat<2>>(x_array, y_array, z_array, dst_channels, _images);
		} break;

		case Image::FORMAT_RF: {
			sample_linear_prepad_unchecked<pixelconv::Direct<float, 1>>(
					x_array, y_array, z_array, dst_channels, _images
			);
		} break;

		default: {
			// Generic and slower
			for (unsigned int i = 0; i < x_array.size(); ++i) {
				const Color c = sample_linear_prepad(Vector3f(x_array[i], y_array[i], z_array[i]));
				for (unsigned int ci = 0; ci < dst_channels.size(); ++ci) {
					if (dst_channels[ci].size() != 0) {
						dst_channels[ci][i] = c[ci];
					}
				}
			}
		} break;
	}
}

// Projects a box onto a [-1..1] quad located at 1 unit on the +X axis, and returns the 2D bounding box of the
// projection in normalized [-1..1] coordinates.
inline Box2f project_box_to_positive_x_side(const Box3f box) {
	// The idea is we implement projection once for the +X side, then we'll get the other 5 sides by swizzling the box.

	// TODO Improve corner cases:
	// When the box is crossing an edge of the "projection pyramid", this code keeps returning a rectangle accounting
	// for the full projection on an infinite plane, which is larger than what's actually projected in -1..1. It remains
	// a valid bounding box, but is not optimal.

	Box2f proj;

	if (box.max.x > 0 && //
		box.max.y > -box.max.x && box.min.y < box.max.x && //
		box.max.z > -box.max.x && box.min.z < box.max.x //
	) {
		if (box.min.y < 0) {
			if (box.min.y < -box.min.x) {
				proj.min.y = -1.f;
			} else {
				proj.min.y = box.min.y / box.min.x;
			}
		} else {
			proj.min.y = box.min.y / box.max.x;
		}

		if (box.max.y < 0) {
			proj.max.y = box.max.y / box.max.x;
		} else {
			if (box.max.y > box.min.x) {
				proj.max.y = 1.f;
			} else {
				proj.max.y = box.max.y / box.min.x;
			}
		}

		if (box.min.z < 0) {
			if (box.min.z < -box.min.x) {
				proj.min.x = -1.f;
			} else {
				proj.min.x = box.min.z / box.min.x;
			}
		} else {
			proj.min.x = box.min.z / box.max.x;
		}

		if (box.max.z < 0) {
			proj.max.x = box.max.z / box.max.x;
		} else {
			if (box.max.z > box.min.x) {
				proj.max.x = 1.f;
			} else {
				proj.max.x = box.max.z / box.min.x;
			}
		}

		// Convert from -1..1 Y-up world space, to 0..1 Y-down texture space (facing out)
		const Box2f snorm = proj;
		proj.min = Vector2f(-snorm.max.x, -snorm.max.y) * 0.5f + Vector2f(0.5f);
		proj.max = Vector2f(-snorm.min.x, -snorm.min.y) * 0.5f + Vector2f(0.5f);

		// proj.clip(Box2f::from_min_max(Vector2f(-1.f), Vector2f(1.f)));
	}

	return proj;
}

// void Cubemap::compute_ranges() {
// 	// TODO
// }

// BoxBounds2i get_pixel_rect_from_snorm(const Box2f box_snorm, const Vector2i res) {
// 	const Vector2f h(0.5f);
// 	const Vector2i minp = to_vec2i(math::floor((h + h * box_snorm.min) * static_cast<float>(res.x)));
// 	const Vector2i maxp = to_vec2i(math::ceil((h + h * box_snorm.max) * static_cast<float>(res.y)));
// 	return BoxBounds2i(minp, maxp).clipped(BoxBounds2i(Vector2i(), res));
// }

BoxBounds2i get_pixel_rect_from_uv(const Box2f uv_box, const Vector2i res) {
	const Vector2i minp = to_vec2i(math::floor(uv_box.min * static_cast<float>(res.x)));
	const Vector2i maxp = to_vec2i(math::ceil(uv_box.max * static_cast<float>(res.y)));
	return BoxBounds2i(minp, maxp).clipped(BoxBounds2i(Vector2i(), res));
}

inline std::array<math::Interval, 4> combine_ranges(
		const std::array<math::Interval, 4> a,
		const std::array<math::Interval, 4> b
) {
	std::array<math::Interval, 4> res;
	for (unsigned int ci = 0; ci < a.size(); ++ci) {
		res[ci] = math::Interval::from_union(a[ci], b[ci]);
	}
	return res;
}

inline Rect2i to_rect2i(const BoxBounds2i box) {
	return Rect2i(box.min_pos, box.max_pos - box.min_pos);
}

inline void combine_box_range_on_image(
		const Box2f uv_box,
		ZN_Cubemap::Range &range,
		const Image &im,
		const uint8_t side_index
) {
	const Vector2i im_size = im.get_size();
	const BoxBounds2i pixel_box = get_pixel_rect_from_uv(uv_box, im_size);
	if (!pixel_box.is_empty()) {
		// TODO: use mipmap ranges to speed this up when the box is large

		// We pad to account for filtering. Do this inside the `if` because otherwise it would always pass, since
		// `uv_box` is actually clamped to edges already...
		// TODO This is kinda shitty. There might be a better way? Also what about cases on the edges?
		const Rect2i padded_pixel_box = to_rect2i(pixel_box.padded(1).clipped(BoxBounds2i(Vector2i(), im_size)));

		if (range.sampled_sides_count == 0) {
			range.combined = godot::ImageUtility::get_range(im, padded_pixel_box);
		} else {
			range.combined = combine_ranges(range.combined, godot::ImageUtility::get_range(im, padded_pixel_box));
		}
		range.sampled_sides[range.sampled_sides_count] = side_index;
		++range.sampled_sides_count;
	}
}

template <int SIDE>
inline Box3f swizzle_box_to_positive_x(const Box3f box) {
	// TODO Any nicer way to prevent the `default` branch from being compiled?
	static_assert(SIDE >= 0 && SIDE < ZN_Cubemap::SIDE_COUNT);

	switch (SIDE) {
		case ZN_Cubemap::SIDE_POSITIVE_X:
			return box;
		case ZN_Cubemap::SIDE_NEGATIVE_X:
			return Box3f::from_min_max(
					Vector3f(-box.max.x, box.min.y, -box.max.z), Vector3f(-box.min.x, box.max.y, -box.min.z)
			);
		case ZN_Cubemap::SIDE_POSITIVE_Y:
			return Box3f::from_min_max(
					Vector3f(box.min.y, -box.max.z, -box.max.x), Vector3f(box.max.y, -box.min.z, -box.min.x)
			);
		case ZN_Cubemap::SIDE_NEGATIVE_Y:
			return Box3f::from_min_max(
					Vector3f(-box.max.y, box.min.z, -box.max.x), Vector3f(-box.min.y, box.max.z, -box.min.x)
			);
		case ZN_Cubemap::SIDE_POSITIVE_Z:
			return Box3f::from_min_max(
					Vector3f(box.min.z, box.min.y, -box.max.x), Vector3f(box.max.z, box.max.y, -box.min.x)
			);
		case ZN_Cubemap::SIDE_NEGATIVE_Z:
			return Box3f::from_min_max(
					Vector3f(-box.max.z, box.min.y, box.min.x), Vector3f(-box.min.z, box.max.y, box.max.x)
			);
		default:
			// Must never happen
			return box;
	}
}

template <int SIDE>
inline void combine_box_range_on_side(
		const Box3f box,
		ZN_Cubemap::Range &range,
		const std::array<Ref<Image>, ZN_Cubemap::SIDE_COUNT> &images
) {
	const Image &im = **images[SIDE];
	const Box2f uv_box = project_box_to_positive_x_side(swizzle_box_to_positive_x<SIDE>(box));
	combine_box_range_on_image(uv_box, range, im, SIDE);
}

ZN_Cubemap::Range ZN_Cubemap::get_range(const Box3f box) const {
	ZN_PROFILE_SCOPE();

	Range range;

	ZN_ASSERT_RETURN_V(is_valid(), range);

	// TODO Cubemap range analysis
	// For each side, project the box from the center of the cubemap into a bounding rectangle on the side (may cover a
	// little more space than the actual projection but should be simpler). Then we can aggregate each side's 2D range
	// using a regular ImageRangeGrid.

	if (box.contains(Vector3f())) {
		// The box contains the center of the cubemap. So any point of any side is in range.
		range.combined = godot::ImageUtility::get_range(**_images[0]);
		for (unsigned int side = 1; side < SIDE_COUNT; ++side) {
			const Image &im = **_images[side];
			range.combined = combine_ranges(range.combined, godot::ImageUtility::get_range(im));
			range.sampled_sides[range.sampled_sides_count] = side;
			++range.sampled_sides_count;
		}
	} else {
		combine_box_range_on_side<SIDE_POSITIVE_X>(box, range, _images);
		combine_box_range_on_side<SIDE_NEGATIVE_X>(box, range, _images);
		combine_box_range_on_side<SIDE_POSITIVE_Y>(box, range, _images);
		combine_box_range_on_side<SIDE_NEGATIVE_Y>(box, range, _images);
		combine_box_range_on_side<SIDE_POSITIVE_Z>(box, range, _images);
		combine_box_range_on_side<SIDE_NEGATIVE_Z>(box, range, _images);
	}

	return range;
}

void ZN_Cubemap::_bind_methods() {
	using Self = ZN_Cubemap;

	ClassDB::bind_method(D_METHOD("create_empty", "resolution", "format"), &Self::create);
	ClassDB::bind_method(D_METHOD("create_from_images", "images"), &Self::create_from_images);
}

} // namespace zylann
