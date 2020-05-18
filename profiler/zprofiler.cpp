#include "zprofiler.h"

#ifdef ZPROFILER_ENABLED
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
bool g_finalized = false;
std::atomic_uint g_buffer_count;

} // namespace

unsigned int ZProfiler::Buffer::get_count() {
	return g_buffer_count;
}

void ZProfiler::terminate() {
	g_shared_mutex.lock();

	g_finalized = true;

	printf("Freeing global profiling data\n");

	if (g_shared_buffer_pool != nullptr) {
		Buffer::delete_recursively(g_shared_buffer_pool);
		g_shared_buffer_pool = nullptr;
	}

	if (g_shared_output != nullptr) {
		Buffer::delete_recursively(g_shared_output);
		g_shared_output = nullptr;
	}

	g_shared_mutex.unlock();

	g_enabled = false;
	// No profiling operations must ever run after this
}

ZProfiler::Buffer *ZProfiler::harvest(Buffer *recycled_buffers) {
	CRASH_COND(g_finalized); // No collection should happen when engine is quitting
	ZProfiler::Buffer *out;

	g_shared_mutex.lock();

	if (g_shared_output != nullptr) {
		CRASH_COND(g_shared_output == g_shared_buffer_pool);
	}

	out = g_shared_output;
	g_shared_output = nullptr;

	if (recycled_buffers != nullptr) {
		CRASH_COND(g_shared_buffer_pool == recycled_buffers);
		// A <-- B <-- C <-- [pool]  <<<  E <-- F <-- G <-- [recycled]
		recycled_buffers->find_last()->prev = g_shared_buffer_pool;
		g_shared_buffer_pool = recycled_buffers;
	}

	g_shared_mutex.unlock();

	return out;
}

void ZProfiler::set_enabled(bool enabled) {
	CRASH_COND(enabled && g_finalized);
	g_enabled = enabled;
}

ZProfiler &ZProfiler::get_thread_profiler() {
	return g_profiler;
}

inline uint64_t get_time() {
	return OS::get_singleton()->get_ticks_usec();
}

ZProfiler::ZProfiler() {
	_enabled = false;
	_profiler_name = String::num_uint64(Thread::get_caller_id());
	_category_stack[0] = CATEGORY_ENGINE;
}

ZProfiler::~ZProfiler() {
	// Thread has stopped
	//flush(false);

	if (_buffer != nullptr) {
		memdelete(_buffer);
		_buffer = nullptr;
	}

	// On Windows, I noticed destructors of a thread_local object were not always called.
	// Like, about 14 instances get created, but only 1 destructor was called.
	// And it might be intentional? I didn't get answers yet :(
	// https://developercommunity.visualstudio.com/content/problem/798234/thread-local.html
	// This is very annoying because each profiler buffers events in their own
	// data storage before they publish it, to reduce overhead.
	// That would have been fine, if there was no StringNames involved.
	// So I try not to rely on destructor to free it, or global data.
	// Instead, it must be freed explicitely when a profiled thread ends, or when the engine quits.
	// Otherwise, it is possible to leak StringNames because this destructor
	// doesn't get called.
}

void ZProfiler::finalize() {
	printf("Finalizing profiler\n");
	CRASH_COND(_finalized); // Don't call twice
	_finalized = true;
	_enabled = false;
	if (_buffer != nullptr) {
		memdelete(_buffer);
		_buffer = nullptr;
	}
}

void ZProfiler::set_thread_name(String name) {
	_profiler_name = String::num_uint64(Thread::get_caller_id());
	_profiler_name += "_";
	_profiler_name += name;

	if (_buffer != nullptr) {
		_buffer->thread_name = _profiler_name;
	}
}

void ZProfiler::begin_sn(StringName description) {
	begin_sn(description, _category_stack[_category_stack_pos]);
}

void ZProfiler::begin_sn(StringName description, uint8_t category) {
	if (!_enabled) {
		return;
	}
	Event e;
	e.type = EVENT_PUSH_SN;
	// StringName does ref-counting with global mutex locking,
	// which adds overhead and is incompatible with the POD nature of event buffers, which uses a union.
	// So we have to freeze it inside a byte array.
	memnew_placement((StringName *)e.description_sn, StringName);
	*(StringName *)e.description_sn = description;
	e.category = category;
	e.relative_time = get_time() - _frame_begin_time;
	push_event(e);
}

void ZProfiler::begin(const char *description) {
	if (!_enabled) {
		return;
	}
	Event e;
	e.type = EVENT_PUSH;
	e.description = description;
	e.category = _category_stack[_category_stack_pos];
	e.relative_time = get_time() - _frame_begin_time;
	push_event(e);
}

void ZProfiler::end() {
	if (!_enabled) {
		return;
	}
	Event e;
	e.type = EVENT_POP;
	e.description = nullptr;
	e.relative_time = get_time() - _frame_begin_time;
	push_event(e);
}

// Every samples taken inside this scope will have the specified category,
// unless overriden with another category call
void ZProfiler::begin_category(uint8_t category) {
	if (!_enabled) {
		return;
	}
	ERR_FAIL_COND(_category_stack_pos + 1 == _category_stack.size());
	++_category_stack_pos;
	_category_stack[_category_stack_pos] = category;
}

void ZProfiler::end_category() {
	if (!_enabled) {
		return;
	}
	ERR_FAIL_COND(_category_stack_pos == 0);
	--_category_stack_pos;
}

void ZProfiler::mark_frame() {
	if (_finalized) {
		return;
	}

	// A thread profiler only changes its enabled state at the frame mark
	if (_enabled != g_enabled) {
		_enabled = !_enabled;

		if (_enabled == false) {
			// Got disabled
			flush(false);
			return;

		} else {
			// Got enabled
			if (_buffer != nullptr) {
				CRASH_COND(_buffer->write_index > 0);
			} else {
				_buffer = memnew(Buffer);
				_buffer->thread_name = _profiler_name;
			}
		}
	} else {
		if (!_enabled) {
			return;
		}
	}

	// We need to flush periodically here,
	// otherwise we would have to wait until the buffer is full
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

	if (g_finalized) {
		// Engine is quitting, don't do anything
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

#endif // ZPROFILER_ENABLED
