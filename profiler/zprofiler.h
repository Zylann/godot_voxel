#ifndef VOXEL_PROFILER_H
#define VOXEL_PROFILER_H

#define VOXEL_PROFILING

#ifdef VOXEL_PROFILING

#include <array>
#include <string>

#define VOXEL_STRINGIFY_(a) #a
#define VOXEL_STRINGIFY(a) VOXEL_STRINGIFY_(a)
#define VOXEL_LINE_STR VOXEL_STRINGIFY(__LINE__)
#define VOXEL_FILE_LINE_STR __FILE__ ": " VOXEL_LINE_STR

#define VOXEL_PROFILE_BEGIN_K(_key) ZProfiler::get_thread_profiler().begin(_key)
#define VOXEL_PROFILE_END_K(_key) ZProfiler::get_thread_profiler().end(_key)
#define VOXEL_PROFILE_SCOPE_K(_scopename, _key) ZProfilerScope _scopename(_key)

#define VOXEL_PROFILE_BEGIN() VOXEL_PROFILE_BEGIN_K(VOXEL_FILE_LINE_STR)
#define VOXEL_PROFILE_END() VOXEL_PROFILE_END_K(VOXEL_FILE_LINE_STR)
#define VOXEL_PROFILE_SCOPE(_scopename) VOXEL_PROFILE_SCOPE_K(_scopename, VOXEL_FILE_LINE_STR)

class ZProfiler {
public:
	ZProfiler();
	~ZProfiler();

	void set_profiler_name(const char *name);
	void begin(const char *description);
	void end();
	void mark_frame();

	enum EventType {
		EVENT_PUSH = 0,
		EVENT_POP,
		EVENT_FRAME
	};

	// 16 bytes
	struct Event {
		uint32_t time;
		uint8_t type;
		const char *description; // TODO Could be StringName?
	};

	struct Buffer {
		std::array<Event, 4096> events;
		unsigned int write_index = 0;
		std::string thread_name;
		Buffer *prev = nullptr;

		inline void reset() {
			write_index = 0;
			prev = nullptr;
		}
	};

	// Gets profiler for the current executing thread so events can be logged
	static ZProfiler &get_thread_profiler();

	// Used from a special thread, from the main harvester only
	static Buffer *harvest(Buffer *recycled_buffers);
	static void set_enabled(bool enabled);
	static uint32_t get_active_profilers_count();

private:
	void push_event(Event e);
	void flush(bool acquire_new_buffer);

	Buffer *_buffer = nullptr;
	std::string _profiler_name;
	bool _enabled = false;
};

struct ZProfilerScope {
	ZProfilerScope(const char *description) {
		ZProfiler::get_thread_profiler().begin(description);
	}
	~ZProfilerScope() {
		ZProfiler::get_thread_profiler().end();
	}
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

#endif // VOXEL_PROFILER_H
