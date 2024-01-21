#include "gpu_task_runner.h"
#include "../../util/dstack.h"
#include "../../util/errors.h"
#include "../../util/godot/classes/rendering_device.h"
#include "../../util/math/funcs.h"
#include "../../util/memory.h"
#include "../../util/profiling.h"

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
	ZN_ASSERT_RETURN(task != nullptr);
	MutexLock mlock(_mutex);
	_shared_tasks.push_back(task);
	_semaphore.post();
	++_pending_count;
}

unsigned int GPUTaskRunner::get_pending_task_count() const {
	return _pending_count;
}

void GPUTaskRunner::thread_func() {
	ZN_PROFILE_SET_THREAD_NAME("Voxel GPU tasks");
	ZN_DSTACK();

	std::vector<IGPUTask *> tasks;

	// We use a common output buffer for tasks that need to download results back to the CPU,
	// because a single call to `buffer_get_data` is cheaper than multiple ones, due to Godot's API being synchronous.
	RID shared_output_storage_buffer_rid;
	unsigned int shared_output_storage_buffer_capacity = 0;
	struct SBRange {
		unsigned int position;
		unsigned int size;
	};
	std::vector<SBRange> shared_output_storage_buffer_segments;

	// Godot does not support async compute, so in order to get results from a compute shader, the only way is to sync
	// with the device, waiting for everything to complete. So instead of running one shader at a time, we run a few of
	// them.
	// It's also unclear how much to execute per frame.
	// 4 tasks was good enough on an nVidia 1060 for detail rendering, but for tasks with different costs it might need
	// different quota to prevent rendering slowdowns...
	const unsigned int batch_count = 16;

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
		GPUTaskContext ctx(*_rendering_device, *_storage_buffer_pool);

		for (size_t begin_index = 0; begin_index < tasks.size(); begin_index += batch_count) {
			ZN_PROFILE_SCOPE_NAMED("Batch");

			const size_t end_index = math::min(begin_index + batch_count, tasks.size());

			unsigned int required_shared_output_buffer_size = 0;
			shared_output_storage_buffer_segments.clear();

			// Get how much data we'll want to download from the GPU for this batch
			for (size_t i = begin_index; i < end_index; ++i) {
				IGPUTask *task = tasks[i];
				const unsigned size = task->get_required_shared_output_buffer_size();
				// TODO Should we pad sections with some kind of alignment?
				shared_output_storage_buffer_segments.push_back(SBRange{ required_shared_output_buffer_size, size });
				required_shared_output_buffer_size += size;
			}

			// Make sure we allocate a storage buffer that can contain all output data in this batch
			if (required_shared_output_buffer_size > shared_output_storage_buffer_capacity) {
				ZN_PROFILE_SCOPE_NAMED("Resize shared output buffer");
				if (shared_output_storage_buffer_rid.is_valid()) {
					free_rendering_device_rid(ctx.rendering_device, shared_output_storage_buffer_rid);
				}
				// TODO Resize to some multiplier above?
				shared_output_storage_buffer_rid =
						ctx.rendering_device.storage_buffer_create(required_shared_output_buffer_size);
				shared_output_storage_buffer_capacity = required_shared_output_buffer_size;
				ZN_ASSERT_CONTINUE(shared_output_storage_buffer_rid.is_valid());
			}

			ctx.shared_output_buffer_rid = shared_output_storage_buffer_rid;

			// Prepare tasks
			for (size_t i = begin_index; i < end_index; ++i) {
				ZN_PROFILE_SCOPE_NAMED("GPU Task Prepare");

				const SBRange range = shared_output_storage_buffer_segments[i - begin_index];
				ctx.shared_output_buffer_begin = range.position;
				ctx.shared_output_buffer_size = range.size;

				IGPUTask *task = tasks[i];
				task->prepare(ctx);
			}

			// Submit work and wait for completion
			{
				ZN_PROFILE_SCOPE_NAMED("RD Submit");
				ctx.rendering_device.submit();
			}
			{
				ZN_PROFILE_SCOPE_NAMED("RD Sync");
				ctx.rendering_device.sync();
			}

			// Download data from shared buffer
			if (required_shared_output_buffer_size > 0 && shared_output_storage_buffer_rid.is_valid()) {
				ZN_PROFILE_SCOPE_NAMED("Download shared output buffer");
				// Unfortunately we can't re-use memory for that buffer, Godot will always want to allocate it using
				// malloc. That buffer can be a few megabytes long...
				ctx.downloaded_shared_output_data = ctx.rendering_device.buffer_get_data(
						shared_output_storage_buffer_rid, 0, required_shared_output_buffer_size);
			}

			// Collect results and complete tasks
			for (size_t i = begin_index; i < end_index; ++i) {
				ZN_PROFILE_SCOPE_NAMED("GPU Task Collect");

				const SBRange range = shared_output_storage_buffer_segments[i - begin_index];
				ctx.shared_output_buffer_begin = range.position;
				ctx.shared_output_buffer_size = range.size;

				IGPUTask *task = tasks[i];
				task->collect(ctx);
				ZN_DELETE(task);
				--_pending_count;
			}

			ctx.downloaded_shared_output_data = PackedByteArray();
		}

		tasks.clear();
	}

	if (shared_output_storage_buffer_rid.is_valid()) {
		free_rendering_device_rid(*_rendering_device, shared_output_storage_buffer_rid);
	}

	ZN_ASSERT(tasks.size() == 0);
}

} // namespace zylann::voxel
