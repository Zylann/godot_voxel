#ifndef ZN_THREADED_TASK_GD
#define ZN_THREADED_TASK_GD

#include "../../godot/classes/ref_counted.h"
#include "../../godot/core/gdvirtual.h"
#include "../threaded_task.h"

namespace zylann {

class ZN_ThreadedTaskInternal;

class ZN_ThreadedTask : public RefCounted {
	GDCLASS(ZN_ThreadedTask, RefCounted)
public:
	void run(int thread_index);
	int get_priority();
	bool is_cancelled();

	// Internal
	bool is_scheduled() const;
	void mark_completed();
	IThreadedTask *create_task();

private:
	GDVIRTUAL1(_run, int);
	GDVIRTUAL0R(int, _get_priority);
	GDVIRTUAL0R(bool, _is_cancelled);

	static void _bind_methods();

	// Created upon scheduling, owned by the task runner
	ZN_ThreadedTaskInternal *_scheduled_task = nullptr;
	bool _completed = false;
};

} // namespace zylann

#endif // ZN_THREADED_TASK_GD
