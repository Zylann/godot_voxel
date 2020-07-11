#ifndef VOXEL_MACROS_H
#define VOXEL_MACROS_H

#include <core/os/os.h>

// print_verbose() is used everywhere in the engine, but its drawback is that even if you turn it off, strings
// you print are still allocated and formatted, to not be used. This macro avoids the string.
#define PRINT_VERBOSE(msg)                          \
	if (OS::get_singleton()->is_stdout_verbose()) { \
		print_line(msg);                            \
	}

#endif // VOXEL_MACROS_H
