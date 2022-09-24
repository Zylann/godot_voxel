#ifndef ZN_GODOT_THREAD_HELPER_H
#define ZN_GODOT_THREAD_HELPER_H

#ifndef ZN_GODOT_EXTENSION
#error "This class is exclusive to Godot Extension"
#endif

#include "../errors.h"
#include "../thread/thread.h"
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace zylann {

// Proxy-object to run a C-style callback using GDExtension threads.
// This class isn't intented to be exposed.
//
// It is recommended to use Godot threads instead of vanilla std::thread,
// because Godot sets up additional stuff in `Thread` (like script debugging and platform-specific stuff to set
// priority). To use Godot threads in GDExtension, you are FORCED to send an object method as callback. And to do that,
// the object must be registered... and it will be exposed. This isn't desired, but there isn't another way AFAIK.
class ZN_GodotThreadHelper : public godot::Object {
	GDCLASS(ZN_GodotThreadHelper, godot::Object)
public:
	ZN_GodotThreadHelper() {}

	void set_callback(Thread::Callback callback, void *data) {
		_callback = callback;
		_callback_data = data;
	}

private:
	void run();

	static void _bind_methods();

	Thread::Callback _callback = nullptr;
	void *_callback_data = nullptr;
};

} // namespace zylann

#endif // ZN_GODOT_THREAD_HELPER_H
