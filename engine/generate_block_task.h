#ifndef GENERATE_BLOCK_TASK_H
#define GENERATE_BLOCK_TASK_H

#include "../util/tasks/async_dependency_tracker.h"
#include "../util/tasks/threaded_task.h"
#include "priority_dependency.h"
#include "streaming_dependency.h"

namespace zylann::voxel {

struct VoxelDataLodMap;

class GenerateBlockTask : public IThreadedTask {
public:
	GenerateBlockTask();
	~GenerateBlockTask();

	void run(ThreadedTaskContext ctx) override;
	int get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	static int debug_get_running_count();

	std::shared_ptr<VoxelBufferInternal> voxels;
	Vector3i position;
	uint32_t volume_id;
	uint8_t lod;
	uint8_t block_size;
	bool has_run = false;
	bool too_far = false;
	bool max_lod_hint = false;
	bool drop_beyond_max_distance = true;
	PriorityDependency priority_dependency;
	std::shared_ptr<StreamingDependency> stream_dependency;
	std::shared_ptr<VoxelDataLodMap> data;
	std::shared_ptr<AsyncDependencyTracker> tracker;
};

} // namespace zylann::voxel

#endif // GENERATE_BLOCK_TASK_H
