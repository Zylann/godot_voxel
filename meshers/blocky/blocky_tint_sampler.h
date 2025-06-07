#ifndef VOXEL_BLOCKY_TINT_SAMPLER_H
#define VOXEL_BLOCKY_TINT_SAMPLER_H

#include "../../util/math/color.h"
#include "../../util/math/vector3i.h"

namespace zylann::voxel {

class VoxelBuffer;

namespace blocky {

struct TintSampler {
	enum Mode {
		MODE_NONE,
		MODE_RAW,
		MODE_COUNT,
	};

	typedef Color (*Callback)(const TintSampler &, const Vector3i);

	const Callback f;
	const VoxelBuffer &voxels;

	static TintSampler create(const VoxelBuffer &p_voxels, const Mode mode);

	inline bool is_valid() const {
		return f != nullptr;
	}

	inline Color evaluate(const Vector3i pos) const {
		if (is_valid()) {
			return (*f)(*this, pos);
		} else {
			return Color(1, 1, 1, 1);
		}
	}
};

} // namespace blocky
} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_TINT_SAMPLER_H
