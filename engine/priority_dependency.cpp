#include "priority_dependency.h"
#include "../constants/voxel_constants.h"
#include "../util/math/funcs.h"

namespace zylann::voxel {

TaskPriority PriorityDependency::evaluate(uint8_t lod_index, uint8_t band2_priority, float *out_closest_distance_sq) {
	TaskPriority priority;
	ERR_FAIL_COND_V(shared == nullptr, priority);

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

	// TODO Any way to optimize out the sqrt? Maybe with a fast integer version?
	// I added it because the LOD modifier was not working with squared distances,
	// which led blocks to subdivide too much compared to their neighbors, making cracks more likely to happen
	const int distance = static_cast<int>(Math::sqrt(closest_distance_sq));

	// TODO Prioritizing LOD makes generation slower... but not prioritizing makes cracks more likely to appear...
	// This could be fixed by allowing the volume to preemptively request blocks of the next LOD?
	//
	// Higher lod indexes come first to allow the octree to subdivide.
	// Then comes distance, which is modified by how much in view the block is
	//priority += (constants::MAX_LOD - lod_index) * 10000;

	// Closer is higher priority. Decreases over distance.
	// Scaled by LOD because we segment priority by LOD too in band 1.
	priority.band0 = math::max(TaskPriority::BAND_MAX - (distance >> (4 + lod_index)), 0);
	// Note: in the past, making lower LOD indices (aka closer detailed ones) have higher priority made cracks between
	// meshes more likely to appear somehow, so for a while I had it inverted. But that priority makes sense so I
	// changed it back. Will see later if that really causes any issue.
	priority.band1 = constants::MAX_LOD - lod_index;
	priority.band2 = band2_priority;
	priority.band3 = constants::TASK_PRIORITY_BAND3_DEFAULT;

	return priority;
}

} // namespace zylann::voxel
