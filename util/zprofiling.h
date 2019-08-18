#ifndef VOXEL_PROFILING_H
#define VOXEL_PROFILING_H

//#define VOXEL_PROFILING

#ifdef VOXEL_PROFILING

#include <core/ustring.h>
#include <array>

#define VOXEL_STRINGIFY_(a) #a
#define VOXEL_STRINGIFY(a) VOXEL_STRINGIFY_(a)
#define VOXEL_LINE_STR VOXEL_STRINGIFY(__LINE__)
#define VOXEL_FILE_LINE_STR __FILE__ ": " VOXEL_LINE_STR

#define VOXEL_PROFILER_DECLARE ZProfiler _zprofiler

#define VOXEL_PROFILE_BEGIN_K(_key) _zprofiler.begin(_key)
#define VOXEL_PROFILE_END_K(_key) _zprofiler.end(_key)
#define VOXEL_PROFILE_SCOPE_K(_scopename, _key) ZProfilerScope _scopename(_zprofiler, _key)

#define VOXEL_PROFILE_BEGIN() VOXEL_PROFILE_BEGIN_K(VOXEL_FILE_LINE_STR)
#define VOXEL_PROFILE_END() VOXEL_PROFILE_END_K(VOXEL_FILE_LINE_STR)
#define VOXEL_PROFILE_SCOPE(_scopename) VOXEL_PROFILE_SCOPE_K(_scopename, VOXEL_FILE_LINE_STR)

class ZProfiler {
public:
	ZProfiler();
	~ZProfiler();

	void set_profiler_name(String name);

	void begin(const char *description);
	void end();

private:
	struct Event {
		uint32_t time;
		uint8_t type;
		const char *description;
	};

	void push_event(Event e);
	void dump();

	enum EventType {
		EVENT_PUSH = 0,
		EVENT_POP
	};

	struct Page {
		std::array<Event, 1024> events;
		unsigned int write_index = 0;
	};

	String _profiler_name;
	std::array<Page *, 1024> _pages;
	int _current_page = 0;
};

struct ZProfilerScope {
	ZProfilerScope(ZProfiler &profiler, const char *description) {
		_profiler = &profiler;
		profiler.begin(description);
	}
	~ZProfilerScope() {
		_profiler->end();
	}
	ZProfiler *_profiler;
};

#else

#define VOXEL_PROFILER_DECLARE //

#define VOXEL_PROFILE_BEGIN_K(_key) //
#define VOXEL_PROFILE_END_K(_key) //
#define VOXEL_PROFILE_SCOPE_K(_scopename, _key) //

#define VOXEL_PROFILE_BEGIN() //
#define VOXEL_PROFILE_END() //
#define VOXEL_PROFILE_SCOPE(_scopename) //

#endif

#endif // VOXEL_PROFILING_H
