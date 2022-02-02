#ifndef ALL_BLOCKS_DATA_REQUEST_H
#define ALL_BLOCKS_DATA_REQUEST_H

#include "../streams/voxel_stream.h"
#include "../util/tasks/threaded_task.h"
#include "streaming_dependency.h"
//#include <memory>

namespace zylann::voxel {

class AllBlocksDataRequest : public IThreadedTask {
public:
	void run(ThreadedTaskContext ctx) override;
	int get_priority() override;
	bool is_cancelled() override;
	void apply_result() override;

	uint32_t volume_id;
	std::shared_ptr<StreamingDependency> stream_dependency;

private:
	VoxelStream::FullLoadingResult _result;
};

} // namespace zylann::voxel

#endif // ALL_BLOCKS_DATA_REQUEST_H
