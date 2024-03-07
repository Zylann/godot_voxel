#ifndef ZN_GODOT_OS_H
#define ZN_GODOT_OS_H

#if defined(ZN_GODOT)
#include <core/os/os.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/os.hpp>
using namespace godot;
#endif

namespace zylann::godot {

inline PackedStringArray get_command_line_arguments() {
#if defined(ZN_GODOT)
	List<String> args_list = OS::get_singleton()->get_cmdline_args();
	PackedStringArray args;
	for (const String &arg : args_list) {
		args.push_back(arg);
	}
	return args;

#elif defined(ZN_GODOT_EXTENSION)
	return OS::get_singleton()->get_cmdline_args();
#endif
}

} // namespace zylann::godot

#endif // ZN_GODOT_OS_H
