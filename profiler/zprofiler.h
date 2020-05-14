#ifndef VOXEL_PROFILER_H
#define VOXEL_PROFILER_H

#define VOXEL_PROFILING

#ifdef VOXEL_PROFILING

#include <core/ustring.h>
#include <array>

// Helpers
// Macros can be tested with gcc and -E option at https://godbolt.org/
#define VOXEL_STRINGIFY_(a) #a
#define VOXEL_STRINGIFY(a) VOXEL_STRINGIFY_(a)
#define VOXEL_LINE_STR VOXEL_STRINGIFY(__LINE__)
#define VOXEL_FILE_LINE_STR __FILE__ ": " VOXEL_LINE_STR
#define VOXEL_COMBINE_NAME_(a, b) a##b
#define VOXEL_COMBINE_NAME(a, b) VOXEL_COMBINE_NAME_(a, b)

// C++ usage macros
#define VOXEL_PROFILE_BEGIN_NAMED(_key) ZProfiler::get_thread_profiler().begin(_key)
#define VOXEL_PROFILE_BEGIN() VOXEL_PROFILE_BEGIN_NAMED(VOXEL_FILE_LINE_STR)
#define VOXEL_PROFILE_END() ZProfiler::get_thread_profiler().end()
#define VOXEL_PROFILE_SCOPE_NAMED(_key) ZProfilerScope VOXEL_COMBINE_NAME(profiler_scope, __LINE__)(_key)
#define VOXEL_PROFILE_SCOPE() VOXEL_PROFILE_SCOPE_NAMED(VOXEL_FILE_LINE_STR)

// Profiler for one thread. Main API to record profiling data.
class ZProfiler {
public:
	ZProfiler();
	~ZProfiler();

	// TODO Rename set_thread_name
	void set_profiler_name(const char *name);
	void begin(const char *description);
	void end();
	void mark_frame();

	// TODO Category events

	enum EventType {
		EVENT_PUSH = 0,
		EVENT_POP,
		EVENT_FRAME
	};

	// 16 bytes
	struct Event {
		union {
			const char *description; // TODO Could be StringName?
			uint64_t time;
		};
		// Time for events inside a frame is stored as 32-bit relative to frame start,
		// because it saves space and should be enough for the durations of a frame.
		// Absolute 64-bit time is only used at FRAME events.
		uint32_t relative_time;
		uint8_t type;
	};

	struct Buffer {
		std::array<Event, 4096> events;
		unsigned int write_index = 0;
		String thread_name;
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
	String _profiler_name;
	bool _enabled = false;
	uint64_t _frame_begin_time = 0;
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

// Empty definitions
#define VOXEL_PROFILE_BEGIN_NAMED(_key) //
#define VOXEL_PROFILE_BEGIN() //
#define VOXEL_PROFILE_END() //
#define VOXEL_PROFILE_SCOPE() //
#define VOXEL_PROFILE_SCOPE_NAMED(_name) //

#endif

#endif // VOXEL_PROFILER_H
