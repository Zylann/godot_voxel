#ifndef ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H
#define ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H

#include "../../util/tasks/threaded_task.h"
#include "../../util/thread/mutex.h"
#include "voxel_instance_generator.h"

#include <memory>

namespace zylann::voxel {

struct VoxelInstanceGeneratorTaskOutput {
	Vector3i render_block_position;
	uint16_t layer_id;
	std::vector<Transform3f> transforms;
};

struct VoxelInstancerGeneratorTaskOutputQueue {
	std::vector<VoxelInstanceGeneratorTaskOutput> results;
	Mutex mutex;
};

// TODO Optimize: eventually this should be moved closer to the meshing task, including edited instances
class GenerateInstancesBlockTask : public IThreadedTask {
public:
	Vector3i mesh_block_grid_position;
	uint16_t layer_id;
	uint8_t lod_index;
	uint8_t gen_octant_mask;
	uint8_t up_mode;
	float mesh_block_size;
	Array surface_arrays;
	Ref<VoxelInstanceGenerator> generator;
	// Can be pre-populated by edited transforms
	std::vector<Transform3f> transforms;
	std::shared_ptr<VoxelInstancerGeneratorTaskOutputQueue> output_queue;

	const char *get_debug_name() const override {
		return "GenerateInstancesBlock";
	}

	void run(ThreadedTaskContext ctx) override;
};

} // namespace zylann::voxel

#endif // ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H
