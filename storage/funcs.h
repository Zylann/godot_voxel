#ifndef VOXEL_STORAGE_FUNCS_H
#define VOXEL_STORAGE_FUNCS_H

#include "../constants/voxel_constants.h"
#include "../util/math/vector3i.h"
#include "../util/span.h"
#include <stdint.h>

namespace zylann::voxel {

inline void clip_copy_region_coord(int &src_min, int &src_max, const int src_size, int &dst_min, const int dst_size) {
	// Clamp source and shrink destination for moved borders
	if (src_min < 0) {
		dst_min += -src_min;
		src_min = 0;
	}
	if (src_max > src_size) {
		src_max = src_size;
	}
	// Clamp destination and shrink source for moved borders
	if (dst_min < 0) {
		src_min += -dst_min;
		dst_min = 0;
	}
	const int dst_w = src_max - src_min;
	const int dst_max = dst_min + dst_w;
	if (dst_max > dst_size) {
		src_max -= dst_max - dst_size;
	}
	// It is possible the source has negative size at this point, which means there is nothing to copy.
	// This must be checked by the caller.
}

// Clips coordinates that may be used to copy a sub-region of a 3D container into another 3D container.
// The result can have zero or negative size, so it must be checked before proceeding.
inline void clip_copy_region(
		Vector3i &src_min, Vector3i &src_max, const Vector3i &src_size, Vector3i &dst_min, const Vector3i &dst_size) {
	clip_copy_region_coord(src_min.x, src_max.x, src_size.x, dst_min.x, dst_size.x);
	clip_copy_region_coord(src_min.y, src_max.y, src_size.y, dst_min.y, dst_size.y);
	clip_copy_region_coord(src_min.z, src_max.z, src_size.z, dst_min.z, dst_size.z);
}

void copy_3d_region_zxy(Span<uint8_t> dst, Vector3i dst_size, Vector3i dst_min, Span<const uint8_t> src,
		Vector3i src_size, Vector3i src_min, Vector3i src_max, size_t item_size);

template <typename T>
inline void copy_3d_region_zxy(Span<T> dst, Vector3i dst_size, Vector3i dst_min, Span<const T> src, Vector3i src_size,
		Vector3i src_min, Vector3i src_max) {
	// The `template` keyword before method name is required when compiling with GCC
	copy_3d_region_zxy(dst.template reinterpret_cast_to<uint8_t>(), dst_size, dst_min,
			src.template reinterpret_cast_to<const uint8_t>(), src_size, src_min, src_max, sizeof(T));
}

template <typename T>
void fill_3d_region_zxy(Span<T> dst, Vector3i dst_size, Vector3i dst_min, Vector3i dst_max, const T value) {
	using namespace math;
	Vector3iUtil::sort_min_max(dst_min, dst_max);
	dst_min.x = clamp(dst_min.x, 0, dst_size.x);
	dst_min.y = clamp(dst_min.y, 0, dst_size.y);
	dst_min.z = clamp(dst_min.z, 0, dst_size.z);
	dst_max.x = clamp(dst_max.x, 0, dst_size.x);
	dst_max.y = clamp(dst_max.y, 0, dst_size.y);
	dst_max.z = clamp(dst_max.z, 0, dst_size.z);
	const Vector3i area_size = dst_max - dst_min;
	if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
		// Degenerate area, we'll not copy anything.
		return;
	}

#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN(Vector3iUtil::get_volume(area_size) <= dst.size());
#endif

	if (area_size == dst_size) {
		for (unsigned int i = 0; i < dst.size(); ++i) {
			dst[i] = value;
		}

	} else {
		const unsigned int dst_row_offset = dst_size.y;
		Vector3i pos;
		for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
			unsigned int dst_ri = Vector3iUtil::get_zxy_index(dst_min + pos, dst_size);
			for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
				// Fill row
				for (pos.y = 0; pos.y < area_size.y; ++pos.y) {
					dst[dst_ri + pos.y] = value;
				}
				dst_ri += dst_row_offset;
			}
		}
	}
}

// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#fundamentals-fixedconv
// Converts an int8 value into a float in the range [-1..1], which includes an exact value for 0.
// -128 is one value of the int8 which will not have a corresponding result, it will be clamped to -1.
inline float s8_to_snorm(int8_t v) {
	return math::max(v / 127.f, -1.f);
}
inline float s8_to_snorm_noclamp(int8_t v) {
	return v / 127.f;
}

// Converts a float value in the range [-1..1] to an int8.
// The float will be clamped if it lies outside of the expected range.
inline int8_t snorm_to_s8(float v) {
	return math::clamp(v, -1.f, 1.f) * 127;
}

// Converts an int8 value into a float in the range [-1..1], which includes an exact value for 0.
// -32767 is one value of the int16 which will not have a corresponding result, it will be clamped to -1.
inline float s16_to_snorm(int16_t v) {
	return math::max(v / 32767.f, -1.f);
}
inline float s16_to_snorm_noclamp(int16_t v) {
	return v / 32767.f;
}

// Converts a float value in the range [-1..1] to an int16.
// The float will be clamped if it lies outside of the expected range.
inline int16_t snorm_to_s16(float v) {
	return math::clamp(v, -1.f, 1.f) * 32767;
}

namespace legacy {

inline float u8_to_snorm(uint8_t v) {
	return (static_cast<float>(v) - 0x7f) * constants::INV_0x7f;
}

inline float u16_to_snorm(uint16_t v) {
	return (static_cast<float>(v) - 0x7fff) * constants::INV_0x7fff;
}

inline uint8_t snorm_to_u8(float v) {
	return zylann::math::clamp(static_cast<int>(128.f * v + 128.f), 0, 0xff);
}

inline uint16_t snorm_to_u16(float v) {
	return zylann::math::clamp(static_cast<int>(0x8000 * v + 0x8000), 0, 0xffff);
}

} // namespace legacy

inline FixedArray<uint8_t, 4> decode_weights_from_packed_u16(uint16_t packed_weights) {
	FixedArray<uint8_t, 4> weights;
	// SIMDable?
	// weights[0] = ((packed_weights >> 0) & 0x0f) << 4;
	// weights[1] = ((packed_weights >> 4) & 0x0f) << 4;
	// weights[2] = ((packed_weights >> 8) & 0x0f) << 4;
	// weights[3] = ((packed_weights >> 12) & 0x0f) << 4;

	// Reduced but not SIMDable
	weights[0] = (packed_weights & 0x0f) << 4;
	weights[1] = packed_weights & 0xf0;
	weights[2] = (packed_weights >> 4) & 0xf0;
	weights[3] = (packed_weights >> 8) & 0xf0;
	// The code above is such that the maximum uint8_t value for a weight is 240, not 255.
	// We could add extra computations in order to match the range exactly,
	// but as a compromise I'm not doing them because it would kinda break bijectivity and is slower.
	// If this is a problem, then it could be an argument to switch to 8bit representation using 3 channels.
	// weights[0] |= weights[0] >> 4;
	// weights[1] |= weights[1] >> 4;
	// weights[2] |= weights[2] >> 4;
	// weights[3] |= weights[3] >> 4;
	return weights;
}

inline FixedArray<uint8_t, 4> decode_indices_from_packed_u16(uint16_t packed_indices) {
	FixedArray<uint8_t, 4> indices;
	// SIMDable?
	indices[0] = (packed_indices >> 0) & 0x0f;
	indices[1] = (packed_indices >> 4) & 0x0f;
	indices[2] = (packed_indices >> 8) & 0x0f;
	indices[3] = (packed_indices >> 12) & 0x0f;
	return indices;
}

inline uint16_t encode_indices_to_packed_u16(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return (a & 0xf) | ((b & 0xf) << 4) | ((c & 0xf) << 8) | ((d & 0xf) << 12);
}

// Encodes from 0..255 to 0..15 values packed in 16 bits. Lower 4 bits of input values will not be preserved.
inline uint16_t encode_weights_to_packed_u16_lossy(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return (a >> 4) | ((b >> 4) << 4) | ((c >> 4) << 8) | ((d >> 4) << 12);
}

// Checks if there are no duplicate indices in any voxel
inline void debug_check_texture_indices(FixedArray<uint8_t, 4> indices) {
	FixedArray<bool, 16> checked;
	fill(checked, false);
	for (unsigned int i = 0; i < indices.size(); ++i) {
		unsigned int ti = indices[i];
		ZN_ASSERT(!checked[ti]);
		checked[ti] = true;
	}
}

struct IntBasis {
	Vector3i x;
	Vector3i y;
	Vector3i z;

	Vector3i get_axis(int i) const {
		// TODO Optimization: could use a union with an array
		switch (i) {
			case Vector3i::AXIS_X:
				return x;
			case Vector3i::AXIS_Y:
				return y;
			case Vector3i::AXIS_Z:
				return z;
			default:
				ZN_CRASH();
		}
		return Vector3i();
	}
};

// Rotates/flips/transposes the contents of a 3D array using a basis.
// Returns the transformed size. Volume remains the same.
// The array's coordinate convention uses ZXY (index+1 does Y+1).
template <typename T>
Vector3i transform_3d_array_zxy(Span<const T> src_grid, Span<T> dst_grid, Vector3i src_size, IntBasis basis) {
	ZN_ASSERT_RETURN_V(Vector3iUtil::is_unit_vector(basis.x), src_size);
	ZN_ASSERT_RETURN_V(Vector3iUtil::is_unit_vector(basis.y), src_size);
	ZN_ASSERT_RETURN_V(Vector3iUtil::is_unit_vector(basis.z), src_size);
	ZN_ASSERT_RETURN_V(src_grid.size() == static_cast<size_t>(Vector3iUtil::get_volume(src_size)), src_size);
	ZN_ASSERT_RETURN_V(dst_grid.size() == static_cast<size_t>(Vector3iUtil::get_volume(src_size)), src_size);

	const int xa = basis.x.x != 0 ? 0 : basis.x.y != 0 ? 1 : 2;
	const int ya = basis.y.x != 0 ? 0 : basis.y.y != 0 ? 1 : 2;
	const int za = basis.z.x != 0 ? 0 : basis.z.y != 0 ? 1 : 2;

	Vector3i dst_size;
	dst_size[xa] = src_size.x;
	dst_size[ya] = src_size.y;
	dst_size[za] = src_size.z;

	// If an axis is negative, it means iteration starts from the end
	const int ox = basis.get_axis(xa).x < 0 ? dst_size.x - 1 : 0;
	const int oy = basis.get_axis(ya).y < 0 ? dst_size.y - 1 : 0;
	const int oz = basis.get_axis(za).z < 0 ? dst_size.z - 1 : 0;

	int src_i = 0;

	for (int z = 0; z < src_size.z; ++z) {
		for (int x = 0; x < src_size.x; ++x) {
			for (int y = 0; y < src_size.y; ++y) {
				// TODO Optimization: can be moved in the outer loop, we only need to add a number to dst_i
				const int dst_x = ox + x * basis.x.x + y * basis.y.x + z * basis.z.x;
				const int dst_y = oy + x * basis.x.y + y * basis.y.y + z * basis.z.y;
				const int dst_z = oz + x * basis.x.z + y * basis.y.z + z * basis.z.z;
				const int dst_i = dst_y + dst_size.y * (dst_x + dst_size.x * dst_z);
				dst_grid[dst_i] = src_grid[src_i];
				++src_i;
			}
		}
	}

	return dst_size;
}

} // namespace zylann::voxel

#endif // VOXEL_STORAGE_FUNCS_H
