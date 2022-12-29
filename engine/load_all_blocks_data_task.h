#ifndef LOAD_ALL_BLOCKS_DATA_TASK_H
#define LOAD_ALL_BLOCKS_DATA_TASK_H

#include "../streams/voxel_stream.h"
#include "../util/tasks/threaded_task.h"
#include "ids.h"
#include "streaming_dependency.h"

namespace zylann::voxel {

class LoadAllBlocksDataTask : public IThreadedTask {
public:
	const char *get_debug_name() const override {
		return "LoadAllBlocksData";
	}

	void run(ThreadedTaskContext ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	VolumeID volume_id;
	std::shared_ptr<StreamingDependency> stream_dependency;

private:
	VoxelStream::FullLoadingResult _result;
};

} // namespace zylann::voxel

#endif // LOAD_ALL_BLOCKS_DATA_TASK_H
