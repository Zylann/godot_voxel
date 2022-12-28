#ifndef VOXEL_SAVE_COMPLETION_TRACKER_H
#define VOXEL_SAVE_COMPLETION_TRACKER_H

#include "../util/godot/classes/ref_counted.h"
#include "../util/tasks/async_dependency_tracker.h"
#include <memory>

namespace zylann::voxel {

// Return value of aynchronous saving functions, allowing to query progress.
// Wraps a task tracker for Godot script API. Might become generic in the future if needed in other places?
class VoxelSaveCompletionTracker : public RefCounted {
	GDCLASS(VoxelSaveCompletionTracker, RefCounted)
public:
	static Ref<VoxelSaveCompletionTracker> create(std::shared_ptr<AsyncDependencyTracker> tracker);

	bool is_complete() const;
	bool is_aborted() const;
	int get_total_tasks() const;
	int get_remaining_tasks() const;

private:
	static void _bind_methods();

	std::shared_ptr<AsyncDependencyTracker> _tracker;
	unsigned int _total_tasks = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_SAVE_COMPLETION_TRACKER_H
