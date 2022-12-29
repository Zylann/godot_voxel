#include "threaded_task_gd.h"

namespace zylann {

// Using a decoupled pattern so we can do a few more safety checks for scripters
class ZN_ThreadedTaskInternal : public IThreadedTask {
public:
	Ref<ZN_ThreadedTask> ref;

	const char *get_debug_name() const override {
		return "ZN_ThreadedTaskInternal";
	}

	void run(ThreadedTaskContext ctx) override {
		ref->run(ctx.thread_index);
	}

	void apply_result() override {
		// Not exposed. Scripters may prefer to use a `completed` signal instead.
		ref->mark_completed();
	}

	TaskPriority get_priority() override {
		TaskPriority priority;
		priority.whole = ref->get_priority();
		return priority;
	}

	bool is_cancelled() override {
		return ref->is_cancelled();
	}
};

void ZN_ThreadedTask::run(int thread_index) {
#ifdef ZN_GODOT
	GDVIRTUAL_CALL(_run, thread_index);
#else
	ERR_PRINT_ONCE("ZN_ThreadedTask::_run is not supported yet in GDExtension!");
#endif
}

int ZN_ThreadedTask::get_priority() {
	int priority = 0;
#ifdef ZN_GODOT
	if (GDVIRTUAL_CALL(_get_priority, priority)) {
		return priority;
	}
#else
	ERR_PRINT_ONCE("ZN_ThreadedTask::_get_priority is not supported yet in GDExtension!");
#endif
	return 0;
}

bool ZN_ThreadedTask::is_cancelled() {
	bool cancelled = false;
#ifdef ZN_GODOT
	if (GDVIRTUAL_CALL(_is_cancelled, cancelled)) {
		return cancelled;
	}
#else
	ERR_PRINT_ONCE("ZN_ThreadedTask::_is_cancelled is not supported yet in GDExtension!");
#endif
	return false;
}

bool ZN_ThreadedTask::is_scheduled() const {
	return _scheduled_task != nullptr;
}

void ZN_ThreadedTask::mark_completed() {
	_scheduled_task = nullptr;
	emit_signal("completed");
}

IThreadedTask *ZN_ThreadedTask::create_task() {
	CRASH_COND(_scheduled_task != nullptr);
	_scheduled_task = memnew(ZN_ThreadedTaskInternal);
	_scheduled_task->ref.reference_ptr(this);
	return _scheduled_task;
}

void ZN_ThreadedTask::_bind_methods() {
	ADD_SIGNAL(MethodInfo("completed"));

#ifdef ZN_GODOT
	GDVIRTUAL_BIND(_run, "thread_index");
	GDVIRTUAL_BIND(_get_priority);
	GDVIRTUAL_BIND(_is_cancelled);
#endif
}

} // namespace zylann
