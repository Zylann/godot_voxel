#include "funcs.h"
#include "../util/math/box3i.h"
#include <cstring>

namespace zylann::voxel {

void copy_3d_region_zxy(Span<uint8_t> dst, Vector3i dst_size, Vector3i dst_min, Span<const uint8_t> src,
		Vector3i src_size, Vector3i src_min, Vector3i src_max, size_t item_size) {
	//
	Vector3iUtil::sort_min_max(src_min, src_max);
	clip_copy_region(src_min, src_max, src_size, dst_min, dst_size);
	const Vector3i area_size = src_max - src_min;
	if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
		// Degenerate area, we'll not copy anything.
		return;
	}

#ifdef DEBUG_ENABLED
	if (src.data() == dst.data()) {
		ZN_ASSERT_RETURN_MSG(
				!Box3i::from_min_max(src_min, src_max).intersects(Box3i::from_min_max(dst_min, dst_min + area_size)),
				"Copy across the same buffer to an overlapping area is not supported");
	}
	ZN_ASSERT_RETURN(Vector3iUtil::get_volume(area_size) * item_size <= dst.size());
	ZN_ASSERT_RETURN(Vector3iUtil::get_volume(area_size) * item_size <= src.size());
#endif

	if (area_size == src_size && area_size == dst_size) {
		// Copy everything
		ZN_ASSERT_RETURN(dst.size() == src.size());
		memcpy(dst.data(), src.data(), dst.size());

	} else {
		// Copy area row by row:
		// This offset is how much to move in order to advance by one row (row direction is Y),
		// essentially doing y+1
		const unsigned int src_row_offset = src_size.y * item_size;
		const unsigned int dst_row_offset = dst_size.y * item_size;
		Vector3i pos;
		for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
			pos.x = 0;
			unsigned int src_ri = Vector3iUtil::get_zxy_index(Vector3i(src_min + pos), src_size) * item_size;
			unsigned int dst_ri = Vector3iUtil::get_zxy_index(Vector3i(dst_min + pos), dst_size) * item_size;
			for (; pos.x < area_size.x; ++pos.x) {
#ifdef DEBUG_ENABLED
				ZN_ASSERT_RETURN(dst_ri < dst.size());
				ZN_ASSERT_RETURN(dst.size() - dst_ri >= area_size.y * item_size);
#endif
				// TODO Cast src and dst to `restrict` so the optimizer can assume adresses don't overlap,
				//      which might allow to write as a for loop (which may compile as a `memcpy`)?
				memcpy(&dst[dst_ri], &src[src_ri], area_size.y * item_size);
				src_ri += src_row_offset;
				dst_ri += dst_row_offset;
			}
		}
	}
}

} // namespace zylann::voxel
