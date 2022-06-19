#ifndef ZN_THREAD_H
#define ZN_THREAD_H

#include <cstdint>

namespace zylann {

struct ThreadImpl;

class Thread {
public:
	enum Priority { //
		PRIORITY_LOW,
		PRIORITY_NORMAL,
		PRIORITY_HIGH
	};

	typedef uint64_t ID;

	typedef void (*Callback)(void *p_userdata);

	Thread();
	~Thread();

	void start(Callback p_callback, void *p_userdata, Priority priority = PRIORITY_NORMAL);
	bool is_started() const;
	void wait_to_finish();

	// Gets a hint of the number of concurrent threads natively supported
	static unsigned int get_hardware_concurrency();

	// Targets the current thread
	static void set_name(const char *name);
	static void sleep_usec(uint32_t microseconds);

	// Get ID of the current thread
	static ID get_caller_id();

private:
	ThreadImpl *_impl = nullptr;
};

} // namespace zylann

#endif // ZN_THREAD_H
