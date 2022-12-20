#ifndef VOXEL_GPU_TASK_RUNNER_H
#define VOXEL_GPU_TASK_RUNNER_H

#include "../util/macros.h"
#include "../util/thread/mutex.h"
#include "../util/thread/semaphore.h"
#include "../util/thread/thread.h"
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
};

class IGPUTask {
public:
	virtual ~IGPUTask() {}

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
};

} // namespace zylann::voxel

#endif // VOXEL_GPU_TASK_RUNNER_H
