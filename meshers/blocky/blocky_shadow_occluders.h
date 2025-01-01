#ifndef VOXEL_BLOCKY_SHADOW_OCCLUDERS_H
#define VOXEL_BLOCKY_SHADOW_OCCLUDERS_H

#include "../../storage/voxel_buffer.h"
#include "../../util/containers/std_vector.h"
#include "../../util/math/vector3f.h"
#include "../../util/math/vector3i.h"
#include "../../util/profiling.h"
#include "blocky_baked_library.h"

namespace zylann::voxel::blocky {

struct OccluderArrays {
	StdVector<Vector3f> vertices;
	StdVector<int32_t> indices;
};

void generate_shadow_occluders(
		OccluderArrays &out_arrays,
		const Span<const uint8_t> id_buffer_raw,
		const VoxelBuffer::Depth depth,
		const Vector3i block_size,
		const BakedLibrary &baked_data,
		const uint8_t enabled_mask
);

} // namespace zylann::voxel::blocky

#endif // VOXEL_BLOCKY_SHADOW_OCCLUDERS_H
