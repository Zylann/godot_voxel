#include "zprofiler.h"

#ifdef VOXEL_PROFILING
#include <core/os/mutex.h>
#include <core/os/os.h>
#include <core/os/thread.h>
#include <atomic>
#include <mutex>

namespace {

thread_local ZProfiler g_profiler;

ZProfiler::Buffer *g_shared_buffer_pool = nullptr;
ZProfiler::Buffer *g_shared_output = nullptr;
std::mutex g_shared_mutex; // TODO Locking is extremely short. Could be a spinlock?
bool g_enabled = false; // TODO Use atomic?
std::atomic_uint g_active_profilers;
std::atomic_uint g_instanced_profilers;

} // namespace

static void delete_recursively(ZProfiler::Buffer *b) {
	while (b != nullptr) {
		CRASH_COND(b == b->prev);
		ZProfiler::Buffer *prev = b->prev;
		memdelete(b);
		b = prev;
	}
}

ZProfiler &ZProfiler::get_thread_profiler() {
	return g_profiler;
}

inline uint64_t get_time() {
	return OS::get_singleton()->get_ticks_usec();
}

ZProfiler::ZProfiler() {
	_profiler_name = String::num_uint64(Thread::get_caller_id());
	_buffer = memnew(Buffer);
	_buffer->thread_name = _profiler_name;
	++g_instanced_profilers;
}

ZProfiler::~ZProfiler() {
	// Thread has stopped
	flush(false);
	--g_instanced_profilers;

	if (_buffer != nullptr) {
		CRASH_COND(_buffer->write_index > 0);
		memdelete(_buffer);
		_buffer = nullptr;
	}

	// TODO Wrong?
	if (g_instanced_profilers == 0) {
		// All profilers gone, free global data
		//printf("Freeing global profiling data\n");
		g_shared_mutex.lock();

		if (g_shared_buffer_pool != nullptr) {
			delete_recursively(g_shared_buffer_pool);
			g_shared_buffer_pool = nullptr;
		}

		if (g_shared_output != nullptr) {
			delete_recursively(g_shared_output);
			g_shared_output = nullptr;
		}

		g_shared_mutex.unlock();
	}
}

void ZProfiler::set_profiler_name(const char *name) {
	_profiler_name = String::num_uint64(Thread::get_caller_id());
	_profiler_name += "_";
	_profiler_name += name;

	if (_buffer != nullptr) {
		_buffer->thread_name = _profiler_name;
	}
}

void ZProfiler::begin(const char *description) {
	if (!_enabled) {
		return;
	}
	Event e;
	e.description = description;
	e.type = EVENT_PUSH;
	e.relative_time = get_time() - _frame_begin_time;
	push_event(e);
}

void ZProfiler::end() {
	if (!_enabled) {
		return;
	}
	Event e;
	e.description = nullptr;
	e.type = EVENT_POP;
	e.relative_time = get_time() - _frame_begin_time;
	push_event(e);
}

void ZProfiler::mark_frame() {
	// A thread profiler only changes its enabled state at the frame mark
	if (_enabled != g_enabled) {
		_enabled = !_enabled;

		if (!_enabled) {
			// Got disabled
			flush(false);
			--g_active_profilers;
			return;

		} else {
			// Got enabled
			if (_buffer != nullptr) {
				CRASH_COND(_buffer->write_index > 0);
			} else {
				_buffer = memnew(Buffer);
				_buffer->thread_name = _profiler_name;
			}
			++g_active_profilers;
		}
	} else {
		if (!_enabled) {
			return;
		}
	}

	// We need to flush periodically here,
	// otherwise we would have to wait until the buffer is full
	// TODO Flush only after a known period of time
	flush(true);

	// TODO Enforce all preceding events were popped
	Event e;
	e.description = nullptr;
	e.type = EVENT_FRAME;
	e.time = get_time();
	_frame_begin_time = e.time;
	push_event(e);
}

void ZProfiler::push_event(Event e) {
	CRASH_COND(_buffer == nullptr);
	CRASH_COND(_buffer->write_index >= _buffer->events.size());

	_buffer->events[_buffer->write_index] = e;
	++_buffer->write_index;

	if (_buffer->write_index == _buffer->events.size()) {
		//printf("ZProfiler end of capacity\n");
		flush(true);
	}
}

void ZProfiler::flush(bool acquire_new_buffer) {
	if (_buffer == nullptr || _buffer->write_index == 0) {
		// Nothing to flush
		return;
	}

	g_shared_mutex.lock();

	// Post buffer
	if (_buffer != nullptr) {
		CRASH_COND(g_shared_output == _buffer);
		_buffer->prev = g_shared_output;
		g_shared_output = _buffer;
		_buffer = nullptr;
	}

	if (acquire_new_buffer) {
		// Get new buffer

		if (g_shared_buffer_pool != nullptr) {
			_buffer = g_shared_buffer_pool;
			CRASH_COND(g_shared_buffer_pool == g_shared_buffer_pool->prev);
			g_shared_buffer_pool = g_shared_buffer_pool->prev;

			g_shared_mutex.unlock();

		} else {
			g_shared_mutex.unlock();

			_buffer = memnew(Buffer);
		}

		_buffer->reset();
		_buffer->thread_name = _profiler_name;

	} else {
		g_shared_mutex.unlock();

		_buffer = nullptr;
	}
}

ZProfiler::Buffer *ZProfiler::harvest(Buffer *recycled_buffers) {
	ZProfiler::Buffer *out;

	g_shared_mutex.lock();

	if (g_shared_output != nullptr) {
		CRASH_COND(g_shared_output == g_shared_buffer_pool);
	}

	out = g_shared_output;
	g_shared_output = nullptr;

	if (recycled_buffers != nullptr) {
		CRASH_COND(g_shared_buffer_pool == recycled_buffers);
		recycled_buffers->prev = g_shared_buffer_pool;
		g_shared_buffer_pool = recycled_buffers;
	}

	g_shared_mutex.unlock();

	return out;
}

void ZProfiler::set_enabled(bool enabled) {
	g_enabled = enabled;
}

uint32_t ZProfiler::get_active_profilers_count() {
	return g_active_profilers;
}

#endif // VOXEL_PROFILER
