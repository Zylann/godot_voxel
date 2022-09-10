#include "voxel_save_completion_tracker.h"

namespace zylann::voxel {

Ref<VoxelSaveCompletionTracker> VoxelSaveCompletionTracker::create(std::shared_ptr<AsyncDependencyTracker> tracker) {
	Ref<VoxelSaveCompletionTracker> self;
	self.instantiate();
	self->_tracker = tracker;
	self->_total_tasks = tracker->get_remaining_count();
	return self;
}

bool VoxelSaveCompletionTracker::is_complete() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, false);
	return _tracker->is_complete();
}

bool VoxelSaveCompletionTracker::is_aborted() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, false);
	return _tracker->is_aborted();
}

int VoxelSaveCompletionTracker::get_total_tasks() const {
	return _total_tasks;
}

int VoxelSaveCompletionTracker::get_remaining_tasks() const {
	ZN_ASSERT_RETURN_V(_tracker != nullptr, 0);
	return _tracker->get_remaining_count();
}

void VoxelSaveCompletionTracker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_complete"), &VoxelSaveCompletionTracker::is_complete);
	ClassDB::bind_method(D_METHOD("is_aborted"), &VoxelSaveCompletionTracker::is_aborted);
	ClassDB::bind_method(D_METHOD("get_total_tasks"), &VoxelSaveCompletionTracker::get_total_tasks);
	ClassDB::bind_method(D_METHOD("get_remaining_tasks"), &VoxelSaveCompletionTracker::get_remaining_tasks);
}

} // namespace zylann::voxel
