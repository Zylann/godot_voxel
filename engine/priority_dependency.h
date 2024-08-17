#ifndef PRIORITY_DEPENDENCY_H
#define PRIORITY_DEPENDENCY_H

#include "../util/containers/std_vector.h"
#include "../util/math/vector3f.h"
#include "../util/tasks/task_priority.h"
#include <atomic>
#include <memory>

namespace zylann::voxel {

// Information to calculate the priority of a voxel task having a specific location
struct PriorityDependency {
	struct ViewersData {
		// These positions are written by the main thread and read by block processing threads.
		// Order doesn't matter.
		// It's only used to adjust task priority so using a lock isn't worth it. In worst case scenario,
		// a task will run much sooner or later than expected, but it will run in any case.
		// This vector is never resized after the instance is created. It is just big enough to have room for all
		// viewers.
		StdVector<Vector3f> viewers;
		// Use this count instead of `viewers.size()`. Can change, but will always be <= `viewers.size()`
		std::atomic_uint32_t viewers_count;
		float highest_view_distance = 999999;
	};

	// TODO If viewers are created at the same time as the first terrain for the first time in a session, loading tasks
	// created that frame will see this list as empty. It happens because that list is not updated in that first frame
	// specifically, in turn because the VoxelEngine singleton is unable to tick at that moment, in turn because it
	// can't register to the main loop before the main scene does its first process, in turn because Godot doesn't
	// expose a way to do so, therefore we had to workaround this by creating a root-level node when the terrain is
	// processed, which can't be done earlier in enter_tree because Godot forbids it. Of course an alternative is to put
	// an autoload in the user's project settings, but that really sucks when Godot has singletons happily ticking out
	// of the box without having to mess with anything in ProjectSettings.
	// Workarounds:
	// - Sync that list when viewers are added, not just when processing
	// - Use explicit cancellation tokens instead for tasks out of range
	std::shared_ptr<ViewersData> shared;
	// Position relative to the same space as viewers.
	// TODO Won't update while in queue. Can it be bad?
	Vector3f world_position;

	// If the closest viewer is further away than this distance, the request can be cancelled as not worth it
	// TODO Move away from this in favor of cancellation tokens,
	// it's not always reliable and requires to handle "task drops" which is annoying
	float drop_distance_squared;

	TaskPriority evaluate(uint8_t lod_index, uint8_t band2_priority, float *out_closest_distance_sq);
};

} // namespace zylann::voxel

#endif // PRIORITY_DEPENDENCY_H
