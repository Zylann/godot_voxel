#include "buffered_task_scheduler.h"
#include "../util/containers/span.h"
#include "../util/errors.h"
#include "../util/io/log.h"
#include "voxel_engine.h"

namespace zylann::voxel {

BufferedTaskScheduler::BufferedTaskScheduler() : _thread_id(Thread::get_caller_id()) {}

BufferedTaskScheduler &BufferedTaskScheduler::get_for_current_thread() {
	static thread_local BufferedTaskScheduler tls_task_scheduler;
	if (tls_task_scheduler.has_tasks()) {
		ZN_PRINT_WARNING("Getting BufferedTaskScheduler for a new batch but it already has tasks!");
	}
	return tls_task_scheduler;
}

void BufferedTaskScheduler::flush() {
	ZN_ASSERT(_thread_id == Thread::get_caller_id());
	if (_main_tasks.size() > 0) {
		VoxelEngine::get_singleton().push_async_tasks(to_span(_main_tasks));
	}
	if (_io_tasks.size() > 0) {
		VoxelEngine::get_singleton().push_async_io_tasks(to_span(_io_tasks));
	}
	_main_tasks.clear();
	_io_tasks.clear();
}

} // namespace zylann::voxel
