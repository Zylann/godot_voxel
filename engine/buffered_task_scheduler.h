#ifndef VOXEL_BUFFERED_TASK_SCHEDULER_H
#define VOXEL_BUFFERED_TASK_SCHEDULER_H

#include "../util/containers/std_vector.h"
#include "../util/thread/thread.h"

namespace zylann {

class IThreadedTask;

namespace voxel {

// Helper class to store tasks and schedule them in a single batch
class BufferedTaskScheduler {
public:
	static BufferedTaskScheduler &get_for_current_thread();

	inline void push_main_task(IThreadedTask *task) {
		_main_tasks.push_back(task);
	}

	inline void push_io_task(IThreadedTask *task) {
		_io_tasks.push_back(task);
	}

	inline unsigned int get_main_count() const {
		return _main_tasks.size();
	}

	inline unsigned int get_io_count() const {
		return _io_tasks.size();
	}

	void flush();

	// No destructor! This does not take ownership, it is only a helper. Flush should be called after each use.

private:
	BufferedTaskScheduler();

	bool has_tasks() const {
		return _main_tasks.size() > 0 || _io_tasks.size() > 0;
	}

	StdVector<IThreadedTask *> _main_tasks;
	StdVector<IThreadedTask *> _io_tasks;
	Thread::ID _thread_id;
};

} // namespace voxel
} // namespace zylann

#endif // VOXEL_BUFFERED_TASK_SCHEDULER_H
