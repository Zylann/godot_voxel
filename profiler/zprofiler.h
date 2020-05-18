#ifndef ZPROFILER_H
#define ZPROFILER_H

#define ZPROFILER_ENABLED

#ifdef ZPROFILER_ENABLED

#include <core/string_name.h>
#include <array>

// Helpers
// Macros can be tested with gcc and -E option at https://godbolt.org/
#define ZPROFILER_STRINGIFY_(a) #a
#define ZPROFILER_STRINGIFY(a) ZPROFILER_STRINGIFY_(a)
#define ZPROFILER_LINE_STR ZPROFILER_STRINGIFY(__LINE__)
#define ZPROFILER_FILE_LINE_STR __FILE__ ": " ZPROFILER_LINE_STR
#define ZPROFILER_COMBINE_NAME_(a, b) a##b
#define ZPROFILER_COMBINE_NAME(a, b) ZPROFILER_COMBINE_NAME_(a, b)

// C++ usage macros (not required, just convenient).
#define ZPROFILER_BEGIN_NAMED(_key) ZProfiler::get_thread_profiler().begin(_key)
#define ZPROFILER_BEGIN() ZPROFILER_BEGIN_NAMED(ZPROFILER_FILE_LINE_STR)
#define ZPROFILER_END() ZProfiler::get_thread_profiler().end()
#define ZPROFILER_SCOPE_NAMED(_key) ZProfilerScope ZPROFILER_COMBINE_NAME(profiler_scope, __LINE__)(_key)
#define ZPROFILER_SCOPE() ZPROFILER_SCOPE_NAMED(ZPROFILER_FILE_LINE_STR)

// Profiler for one thread. Main API to record profiling data.
class ZProfiler {
public:
	static const uint32_t MAX_STACK = 64;

	enum EventType {
		EVENT_PUSH = 0,
		EVENT_PUSH_SN,
		EVENT_POP,
		EVENT_FRAME
	};

	// Having string categories is possible,
	// but I figured this is enough for now
	enum Category {
		CATEGORY_ENGINE = 0, // Default, since all code starts from engine
		CATEGORY_SCRIPT,
		CATEGORY_COUNT
	};

	// 16 bytes POD, there can be a LOT of these
	struct Event {
		union {
			const char *description;
			uint8_t description_sn[sizeof(StringName)];
			uint64_t time;
		};
		// Time for events inside a frame is stored as 32-bit relative to frame start,
		// because it saves space and should be enough for the durations of a frame.
		// Absolute 64-bit time is only used at FRAME events.
		uint32_t relative_time;
		uint8_t type;
		uint8_t category;
	};

	static_assert(sizeof(Event) <= 16, "Event is expected to be no more than 16 bytes");

	// Events are written into a fixed-size buffer, which has lower overhead.
	// When full, or when the frame ends, buffers are posted to a shared location (single, short sync point),
	// where they can be picked up by the reader for processing.
	struct Buffer {
		std::array<Event, 4096> events;
		unsigned int write_index = 0;
		String thread_name;
		Buffer *prev = nullptr;

		~Buffer() {
			reset();
		}

		inline void reset_no_string_names() {
			write_index = 0;
			prev = nullptr;
		}

		inline void reset() {
			// We have to do this JUST because of StringName...
			for (int i = 0; i < write_index; ++i) {
				if (events[i].type == EVENT_PUSH_SN) {
					StringName *sn = (StringName *)events[i].description_sn;
					sn->~StringName();
				}
			}

			write_index = 0;
			prev = nullptr;
		}

		inline Buffer *find_last() {
			Buffer *last = this;
			Buffer *p = prev;
			while (p != nullptr) {
				last = p;
				p = p->prev;
			}
			return last;
		}

		static void delete_recursively(ZProfiler::Buffer *b) {
			while (b != nullptr) {
				CRASH_COND(b == b->prev);
				Buffer *prev = b->prev;
				memdelete(b);
				b = prev;
			}
		}
	};

	ZProfiler();
	~ZProfiler();

	void set_thread_name(String name);
	void begin_sn(StringName description); // For scripts, which can't provide a const char*
	void begin_sn(StringName description, uint8_t category);
	void begin(const char *description); // For C++, where litterals just work
	void begin_category(uint8_t category);
	void end_category();
	void end();
	void mark_frame();

	// Cleanups the profiler of the current thread, it won't record anything ever again.
	// Due to thread_local destructors not always getting called (??),
	// it is recommended to call this at the end of a thread.
	void finalize();

	// Gets profiler for the current executing thread so events can be logged
	static ZProfiler &get_thread_profiler();

	// Used from a special thread, from ProfilingServer only
	static Buffer *harvest(Buffer *recycled_buffers);
	static void set_enabled(bool enabled);

	// Declares that all profiling is to be stopped forever.
	// This will cleanup global state and any profiler still active will stop posting results.
	static void terminate();

private:
	void push_event(Event e);
	void flush(bool acquire_new_buffer);

	Buffer *_buffer = nullptr;
	String _profiler_name;
	bool _enabled = false;
	bool _finalized = false;
	uint64_t _frame_begin_time = 0;
	std::array<uint8_t, MAX_STACK> _category_stack;
	uint32_t _category_stack_pos = 0;
};

struct ZProfilerScope {
	ZProfilerScope(const char *description) {
		ZProfiler::get_thread_profiler().begin(description);
	}
	~ZProfilerScope() {
		ZProfiler::get_thread_profiler().end();
	}
};

struct ZProfilerScopeSN {
	ZProfilerScopeSN(StringName description) {
		ZProfiler::get_thread_profiler().begin_sn(description);
	}
	ZProfilerScopeSN(StringName description, uint8_t category) {
		ZProfiler::get_thread_profiler().begin_sn(description, category);
	}
	~ZProfilerScopeSN() {
		ZProfiler::get_thread_profiler().end();
	}
};

struct ZProfilerCategoryScope {
	ZProfilerCategoryScope(uint8_t category) {
		ZProfiler::get_thread_profiler().begin_category(category);
	}
	~ZProfilerCategoryScope() {
		ZProfiler::get_thread_profiler().end_category();
	}
};

#else

// Empty definitions
#define ZPROFILER_BEGIN_NAMED(_key) //
#define ZPROFILER_BEGIN() //
#define ZPROFILER_END() //
#define ZPROFILER_SCOPE() //
#define ZPROFILER_SCOPE_NAMED(_name) //

#endif

#endif // ZPROFILER_H
