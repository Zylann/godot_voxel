#include "thread.h"
#include "../memory.h"

#include <core/os/os.h>
#include <core/os/thread.h>
#include <core/string/ustring.h>

namespace zylann {

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
