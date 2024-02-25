#ifndef VOXEL_LOAD_INSTANCE_BLOCK_TASK_H
#define VOXEL_LOAD_INSTANCE_BLOCK_TASK_H

#include "../../streams/voxel_stream.h"
#include "../../util/godot/core/array.h"
#include "../../util/math/vector3i.h"
#include "../../util/tasks/threaded_task.h"
#include "voxel_instance_library.h"
#include "voxel_instancer_task_output_queue.h"
#include <cstdint>
#include <memory>

namespace zylann::voxel {

struct VoxelInstancerQuickReloadingCache;

// Loads all instances of all layers of a specific LOD in a specific chunk
class LoadInstanceChunkTask : public IThreadedTask {
public:
	LoadInstanceChunkTask( //
			std::shared_ptr<VoxelInstancerTaskOutputQueue> output_queue, //
			Ref<VoxelStream> stream, //
			std::shared_ptr<VoxelInstancerQuickReloadingCache> quick_reload_cache,
			Ref<VoxelInstanceLibrary> library, //
			Array mesh_arrays, //
			Vector3i grid_position, //
			uint8_t lod_index, //
			uint8_t instance_block_size, //
			uint8_t data_block_size, //
			uint8_t up_mode //
	);

	const char *get_debug_name() const override {
		return "LoadInstanceChunk";
	}

	void run(ThreadedTaskContext &ctx) override;

private:
	std::shared_ptr<VoxelInstancerTaskOutputQueue> _output_queue;
	Ref<VoxelStream> _stream;
	std::shared_ptr<VoxelInstancerQuickReloadingCache> _quick_reload_cache;
	Ref<VoxelInstanceLibrary> _library;
	Array _mesh_arrays;
	Vector3i _render_grid_position;
	uint8_t _lod_index;
	uint8_t _instance_block_size;
	uint8_t _data_block_size;
	uint8_t _up_mode;
};

} // namespace zylann::voxel

#endif // VOXEL_LOAD_INSTANCE_BLOCK_TASK_H
