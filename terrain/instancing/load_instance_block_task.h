#ifndef VOXEL_LOAD_INSTANCE_BLOCK_TASK_H
#define VOXEL_LOAD_INSTANCE_BLOCK_TASK_H

#include "../../streams/voxel_stream.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/vector3i.h"
#include "../../util/tasks/threaded_task.h"
#include "instancer_task_output_queue.h"
#include "up_mode.h"
#include "voxel_instance_library.h"
#include <cstdint>
#include <memory>

namespace zylann::voxel {

struct InstancerQuickReloadingCache;

// Loads all instances of all layers of a specific LOD in a specific chunk
class LoadInstanceChunkTask : public IThreadedTask {
public:
	LoadInstanceChunkTask( //
			std::shared_ptr<InstancerTaskOutputQueue> output_queue, //
			Ref<VoxelStream> stream, //
			std::shared_ptr<InstancerQuickReloadingCache> quick_reload_cache,
			Ref<VoxelInstanceLibrary> library, //
			Array mesh_arrays, //
			Vector3i grid_position, //
			uint8_t lod_index, //
			uint8_t instance_block_size, //
			uint8_t data_block_size, //
			UpMode up_mode //
	);

	const char *get_debug_name() const override {
		return "LoadInstanceChunk";
	}

	void run(ThreadedTaskContext &ctx) override;

private:
	std::shared_ptr<InstancerTaskOutputQueue> _output_queue;
	Ref<VoxelStream> _stream;
	std::shared_ptr<InstancerQuickReloadingCache> _quick_reload_cache;
	Ref<VoxelInstanceLibrary> _library;
	Array _mesh_arrays;
	Vector3i _render_grid_position;
	uint8_t _lod_index;
	uint8_t _instance_block_size;
	uint8_t _data_block_size;
	UpMode _up_mode;
};

} // namespace zylann::voxel

#endif // VOXEL_LOAD_INSTANCE_BLOCK_TASK_H
