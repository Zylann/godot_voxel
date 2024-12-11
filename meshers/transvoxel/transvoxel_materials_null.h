#ifndef VOXEL_TRANSVOXEL_MATERIALS_NULL_H
#define VOXEL_TRANSVOXEL_MATERIALS_NULL_H

#include "transvoxel.h"

namespace zylann::voxel::transvoxel::materials {

struct NullProcessor {
	// Called for every 2x2x2 cell containing triangles.
	// The returned value is used to determine if the next cell can re-use vertices from previous cells, when equal.
	inline uint32_t on_cell(const FixedArray<uint32_t, 8> &corner_voxel_indices, const uint8_t case_code) const {
		return 0;
	}
	// Called for every 2x3x3 transition cell containing triangles.
	// Such cells are actually in 2D data-wise, so corners are the same value, so only 9 are passed in.
	// The returned value is used to determine if the next cell can re-use vertices from previous cells, when equal.
	inline uint32_t on_transition_cell(const FixedArray<uint32_t, 9> &corner_voxel_indices, const uint8_t case_code)
			const {
		return 0;
	}
	// Called one or more times after each `on_cell` for every new vertex, to interpolate and add material data
	inline void on_vertex(const unsigned int v0, const unsigned int v1, const float alpha) const {
		return;
	}
};

} // namespace zylann::voxel::transvoxel::materials

#endif // VOXEL_TRANSVOXEL_MATERIALS_NULL_H
