#ifndef VOXEL_STORAGE_FUNCS_H
#define VOXEL_STORAGE_FUNCS_H

#include "../util/array_slice.h"
#include "../util/math/vector3i.h"

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

void copy_3d_region_zxy(
		ArraySlice<uint8_t> dst, Vector3i dst_size, Vector3i dst_min,
		ArraySlice<const uint8_t> src, Vector3i src_size, Vector3i src_min, Vector3i src_max,
		size_t item_size);

template <typename T>
inline void copy_3d_region_zxy(
		ArraySlice<T> dst, Vector3i dst_size, Vector3i dst_min,
		ArraySlice<const T> src, Vector3i src_size, Vector3i src_min, Vector3i src_max) {
	copy_3d_region_zxy(
			dst.reinterpret_cast_to<uint8_t>(), dst_size, dst_min,
			src.reinterpret_cast_to<const uint8_t>(), src_size, src_min, src_max,
			sizeof(T));
}

template <typename T>
void fill_3d_region_zxy(ArraySlice<T> dst, Vector3i dst_size, Vector3i dst_min, Vector3i dst_max, const T value) {
	Vector3i::sort_min_max(dst_min, dst_max);
	dst_min.x = clamp(dst_min.x, 0, dst_size.x);
	dst_min.y = clamp(dst_min.y, 0, dst_size.y);
	dst_min.z = clamp(dst_min.z, 0, dst_size.z);
	dst_max.x = clamp(dst_max.x, 0, dst_size.x);
	dst_max.y = clamp(dst_max.y, 0, dst_size.y);
	dst_max.z = clamp(dst_max.z, 0, dst_size.z);
	const Vector3i area_size = src_max - src_min;
	if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
		// Degenerate area, we'll not copy anything.
		return;
	}

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(area_size.volume() > dst.size());
#endif

	if (area_size == dst_size) {
		for (unsigned int i = 0; i < dst.size(); ++i) {
			dst[i] = value;
		}

	} else {
		const unsigned int dst_row_offset = dst_size.y;
		Vector3i pos;
		for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
			unsigned int dst_ri = Vector3i(dst_min + pos).get_zxy_index(src_size);
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

inline uint16_t encode_weights_to_packed_u16(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
	return (a >> 4) | ((b >> 4) << 4) | ((c >> 4) << 8) | ((d >> 4) << 12);
}

// Checks if there are no duplicate indices in any voxel
inline void debug_check_texture_indices(FixedArray<uint8_t, 4> indices) {
	FixedArray<bool, 16> checked;
	checked.fill(false);
	for (int i = 0; i < indices.size(); ++i) {
		unsigned int ti = indices[i];
		CRASH_COND(checked[ti]);
		checked[ti] = true;
	}
}

#endif // VOXEL_STORAGE_FUNCS_H
