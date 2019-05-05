#ifndef PROFILING_CLOCK_H
#define PROFILING_CLOCK_H

#include <core/os/os.h>

struct ProfilingClock {
	uint64_t time_before = 0;

	ProfilingClock() {
		restart();
	}

	uint64_t restart() {
		uint64_t now = OS::get_singleton()->get_ticks_usec();
		uint64_t time_spent = now - time_before;
		time_before = OS::get_singleton()->get_ticks_usec();
		return time_spent;
	}
};

#endif // PROFILING_CLOCK_H
