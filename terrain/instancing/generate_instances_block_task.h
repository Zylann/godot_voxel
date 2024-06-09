#ifndef ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H
#define ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H

#include "../../util/containers/std_vector.h"
#include "../../util/godot/core/array.h"
#include "../../util/tasks/threaded_task.h"
#include "instancer_task_output_queue.h"
#include "up_mode.h"
#include "voxel_instance_generator.h"

#include <cstdint>
#include <memory>

namespace zylann::voxel {

// TODO Optimize: eventually this should be moved closer to the meshing task, including edited instances
class GenerateInstancesBlockTask : public IThreadedTask {
public:
	Vector3i mesh_block_grid_position;
	uint16_t layer_id;
	uint8_t lod_index;
	uint8_t edited_mask;
	UpMode up_mode;
	float mesh_block_size;
	Array surface_arrays;
	Ref<VoxelInstanceGenerator> generator;
	// Can be pre-populated by edited transforms
	StdVector<Transform3f> transforms;
	std::shared_ptr<InstancerTaskOutputQueue> output_queue;

	const char *get_debug_name() const override {
		return "GenerateInstancesBlock";
	}

	void run(ThreadedTaskContext &ctx) override;
};

} // namespace zylann::voxel

#endif // ZN_VOXEL_GENERATE_INSTANCES_BLOCK_TASK_H
