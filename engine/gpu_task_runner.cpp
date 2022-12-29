#include "gpu_task_runner.h"
#include "../util/dstack.h"
#include "../util/errors.h"
#include "../util/godot/classes/rendering_device.h"
#include "../util/math/funcs.h"
#include "../util/memory.h"
#include "../util/profiling.h"

namespace zylann::voxel {

GPUTaskRunner::GPUTaskRunner() {}

GPUTaskRunner::~GPUTaskRunner() {
	stop();

	for (IGPUTask *task : _shared_tasks) {
		ZN_DELETE(task);
	}
}

void GPUTaskRunner::start(RenderingDevice *rd, GPUStorageBufferPool *pool) {
	ZN_ASSERT(rd != nullptr);
	ZN_ASSERT(pool != nullptr);
	ZN_ASSERT(!_running);
	_rendering_device = rd;
	_storage_buffer_pool = pool;
	_running = true;
	_thread.start(
			[](void *p_userdata) {
				GPUTaskRunner *runner = static_cast<GPUTaskRunner *>(p_userdata);
				runner->thread_func();
			},
			this);
}

void GPUTaskRunner::stop() {
	if (!_running) {
		ZN_PRINT_VERBOSE("GPUTaskRunner::stop() was called but it wasn't running.");
		return;
	}
	_running = false;
	_semaphore.post();
	_thread.wait_to_finish();
}

void GPUTaskRunner::push(IGPUTask *task) {
	MutexLock mlock(_mutex);
	_shared_tasks.push_back(task);
	_semaphore.post();
}

void GPUTaskRunner::thread_func() {
	ZN_PROFILE_SET_THREAD_NAME("Voxel GPU tasks");
	ZN_DSTACK();

	std::vector<IGPUTask *> tasks;

	// Godot does not support async compute, so in order to get results from a compute shader, the only way is to sync
	// with the device, waiting for everything to complete. So instead of running one shader at a time, we run a few of
	// them.
	const unsigned int batch_count = 4;

	while (_running) {
		{
			MutexLock mlock(_mutex);
			tasks = std::move(_shared_tasks);
		}
		if (tasks.size() == 0) {
			_semaphore.wait();
			continue;
		}

		ZN_ASSERT(_rendering_device != nullptr);
		GPUTaskContext ctx{ *_rendering_device, *_storage_buffer_pool };

		for (size_t begin_index = 0; begin_index < tasks.size(); begin_index += batch_count) {
			const size_t end_index = math::min(begin_index + batch_count, tasks.size());

			for (size_t i = begin_index; i < end_index; ++i) {
				ZN_PROFILE_SCOPE_NAMED("GPU Task Prepare");
				IGPUTask *task = tasks[i];
				task->prepare(ctx);
			}

			{
				ZN_PROFILE_SCOPE_NAMED("RD Submit");
				ctx.rendering_device.submit();
			}
			{
				ZN_PROFILE_SCOPE_NAMED("RD Sync");
				ctx.rendering_device.sync();
			}

			for (size_t i = begin_index; i < end_index; ++i) {
				ZN_PROFILE_SCOPE_NAMED("GPU Task Collect");
				IGPUTask *task = tasks[i];
				task->collect(ctx);
				ZN_DELETE(task);
			}
		}

		tasks.clear();
	}

	ZN_ASSERT(tasks.size() == 0);
}

} // namespace zylann::voxel
