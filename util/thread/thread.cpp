#include "thread.h"
#include "../godot/classes/os.h"
#include "../memory.h"

#if defined(ZN_GODOT)
#include <core/os/thread.h>
#elif defined(ZN_GODOT_EXTENSION)
#include "godot_thread_helper.h"
#include <godot_cpp/classes/thread.hpp>
// using namespace godot;
#endif

#include <thread>

namespace zylann {

#if defined(ZN_GODOT)

struct ThreadImpl {
	::Thread thread;
};

Thread::Thread() {
	_impl = ZN_NEW(ThreadImpl);
}

Thread::~Thread() {
	ZN_DELETE(_impl);
}

void Thread::start(Callback p_callback, void *p_userdata, Priority priority) {
	::Thread::Settings settings;
	settings.priority = ::Thread::Priority(priority);
	_impl->thread.start(p_callback, p_userdata, settings);
}

bool Thread::is_started() const {
	return _impl->thread.is_started();
}

void Thread::wait_to_finish() {
	_impl->thread.wait_to_finish();
}

void Thread::set_name(const char *name) {
	::Thread::set_name(String(name));
}

#elif defined(ZN_GODOT_EXTENSION)

struct ThreadImpl {
	godot::Ref<godot::Thread> thread;
	ZN_GodotThreadHelper *helper;

	ThreadImpl() {
		thread.instantiate();
		helper = memnew(ZN_GodotThreadHelper);
	}

	~ThreadImpl() {
		memdelete(helper);
	}
};

Thread::Thread() {
	_impl = ZN_NEW(ThreadImpl);
}

Thread::~Thread() {
	ZN_DELETE(_impl);
}

void Thread::start(Callback p_callback, void *p_userdata, Priority priority) {
	godot::Thread::Priority gd_priority = godot::Thread::Priority(priority);
	_impl->helper->set_callback(p_callback, p_userdata);
	_impl->thread->start(godot::Callable(_impl->helper, godot::StringName("run")), gd_priority);
}

bool Thread::is_started() const {
	return _impl->thread->is_started();
}

void Thread::wait_to_finish() {
	_impl->thread->wait_to_finish();
}

void Thread::set_name(const char *name) {
	// TODO GDExtension is not exposing a way to set the name of the current thread.
}

#endif

void Thread::sleep_usec(uint32_t microseconds) {
	OS::get_singleton()->delay_usec(microseconds);
}

unsigned int Thread::get_hardware_concurrency() {
	return std::thread::hardware_concurrency();
}

static uint64_t get_hash(const std::thread::id &p_t) {
	static std::hash<std::thread::id> hasher;
	return hasher(p_t);
}

Thread::ID Thread::get_caller_id() {
	static thread_local ID caller_id = get_hash(std::this_thread::get_id());
	return caller_id;
}

} // namespace zylann
