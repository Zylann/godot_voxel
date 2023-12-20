#ifndef LOAD_ALL_BLOCKS_DATA_TASK_H
#define LOAD_ALL_BLOCKS_DATA_TASK_H

#include "../engine/ids.h"
#include "../engine/streaming_dependency.h"
#include "../util/tasks/threaded_task.h"
#include "voxel_stream.h"

namespace zylann::voxel {

class VoxelData;

class LoadAllBlocksDataTask : public IThreadedTask {
public:
	const char *get_debug_name() const override {
		return "LoadAllBlocksData";
	}

	void run(ThreadedTaskContext &ctx) override;
	TaskPriority get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	VolumeID volume_id;
	std::shared_ptr<StreamingDependency> stream_dependency;
	std::shared_ptr<VoxelData> data;

private:
	VoxelStream::FullLoadingResult _result;
};

} // namespace zylann::voxel

#endif // LOAD_ALL_BLOCKS_DATA_TASK_H
