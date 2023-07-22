#ifndef VOXEL_GPU_TASK_RUNNER_H
#define VOXEL_GPU_TASK_RUNNER_H

#include "../../util/godot/core/packed_byte_array.h"
#include "../../util/godot/core/rid.h"
#include "../../util/godot/macros.h"
#include "../../util/macros.h"
#include "../../util/span.h"
#include "../../util/thread/mutex.h"
#include "../../util/thread/semaphore.h"
#include "../../util/thread/thread.h"
#include <atomic>
#include <vector>

ZN_GODOT_FORWARD_DECLARE(class RenderingDevice)
#ifdef ZN_GODOT_EXTENSION
using namespace godot;
#endif

namespace zylann::voxel {

class GPUStorageBufferPool;

struct GPUTaskContext {
	RenderingDevice &rendering_device;
	GPUStorageBufferPool &storage_buffer_pool;

	// Buffer shared by multiple tasks in the current batch.
	// It will be downloaded in one go before collection, which is faster than downloading multiple individual buffers,
	// due to Godot's API only exposing blocking calls.
	unsigned int shared_output_buffer_begin = 0; // In bytes
	unsigned int shared_output_buffer_size = 0; // In bytes
	RID shared_output_buffer_rid;
	PackedByteArray downloaded_shared_output_data;

	GPUTaskContext(RenderingDevice &rd, GPUStorageBufferPool &sb_pool) :
			rendering_device(rd), storage_buffer_pool(sb_pool) {}
};

class IGPUTask {
public:
	virtual ~IGPUTask() {}

	virtual unsigned int get_required_shared_output_buffer_size() const {
		return 0;
	}

	virtual void prepare(GPUTaskContext &ctx) = 0;
	virtual void collect(GPUTaskContext &ctx) = 0;
};

// Runs tasks that schedules compute shaders and collects their results.
class GPUTaskRunner {
public:
	GPUTaskRunner();
	~GPUTaskRunner();

	void start(RenderingDevice *rd, GPUStorageBufferPool *pool);
	void stop();
	void push(IGPUTask *task);
	unsigned int get_pending_task_count() const;

private:
	void thread_func();

	RenderingDevice *_rendering_device = nullptr;
	GPUStorageBufferPool *_storage_buffer_pool = nullptr;
	std::vector<IGPUTask *> _shared_tasks;
	Mutex _mutex;
	Semaphore _semaphore;
	// Using a thread because so far it looks like the only way to submit and receive data with RenderingDevice is to
	// block the calling thread and wait for the graphics card...
	// Since we already have a thread pool, this thread is supposed to be mostly sleeping or waiting.
	Thread _thread;
	bool _running = false;
	std::atomic_uint32_t _pending_count = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_GPU_TASK_RUNNER_H
