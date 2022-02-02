#include "priority_dependency.h"
#include "../constants/voxel_constants.h"

namespace zylann::voxel {

int PriorityDependency::evaluate(uint8_t lod_index, float *out_closest_distance_sq) {
	ERR_FAIL_COND_V(shared == nullptr, 0);

	const std::vector<Vector3> &viewer_positions = shared->viewers;
	const Vector3 block_position = world_position;

	float closest_distance_sq = 99999.f;
	if (viewer_positions.size() == 0) {
		// Assume origin
		closest_distance_sq = block_position.length_squared();
	} else {
		for (size_t i = 0; i < viewer_positions.size(); ++i) {
			float d = viewer_positions[i].distance_squared_to(block_position);
			if (d < closest_distance_sq) {
				closest_distance_sq = d;
			}
		}
	}

	if (out_closest_distance_sq != nullptr) {
		*out_closest_distance_sq = closest_distance_sq;
	}

	// TODO Any way to optimize out the sqrt?
	// I added it because the LOD modifier was not working with squared distances,
	// which led blocks to subdivide too much compared to their neighbors, making cracks more likely to happen
	int priority = static_cast<int>(Math::sqrt(closest_distance_sq));

	// TODO Prioritizing LOD makes generation slower... but not prioritizing makes cracks more likely to appear...
	// This could be fixed by allowing the volume to preemptively request blocks of the next LOD?
	//
	// Higher lod indexes come first to allow the octree to subdivide.
	// Then comes distance, which is modified by how much in view the block is
	priority += (constants::MAX_LOD - lod_index) * 10000;

	return priority;
}

} // namespace zylann::voxel
