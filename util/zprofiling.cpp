#include "zprofiling.h"

#ifdef VOXEL_PROFILING
#include <core/hash_map.h>
#include <core/os/os.h>

inline uint64_t get_time() {
	return OS::get_singleton()->get_ticks_usec();
	//return OS::get_singleton()->get_ticks_msec();
}

ZProfiler::ZProfiler() {
	for (int i = 0; i < _pages.size(); ++i) {
		_pages[i] = new Page();
	}
}

ZProfiler::~ZProfiler() {
	dump();
	for (int i = 0; i < _pages.size(); ++i) {
		delete _pages[i];
	}
}

void ZProfiler::set_profiler_name(String name) {
	_profiler_name = name;
}

void ZProfiler::begin(const char *description) {
	Event e;
	e.description = description;
	e.type = EVENT_PUSH;
	e.time = get_time();
	push_event(e);
}

void ZProfiler::end() {
	Event e;
	e.description = nullptr;
	e.type = EVENT_POP;
	e.time = get_time();
	push_event(e);
}

void ZProfiler::push_event(Event e) {

	if (_current_page < _pages.size()) {

		Page &page = *_pages[_current_page];
		page.events[page.write_index] = e;
		++page.write_index;

		if (page.write_index >= page.events.size()) {
			++_current_page;
			if (_current_page >= _pages.size()) {
				print_error("ZProfiler end of capacity");
			}
		}
	}
}

void ZProfiler::dump() {

	print_line("Dumping ZProfiler data");

	unsigned short next_index = 1;
	HashMap<const char *, unsigned short> string_index;

	for (int i = 0; i < _pages.size(); ++i) {
		const Page &page = *_pages[i];

		for (int j = 0; j < page.events.size(); ++j) {
			const Event &event = page.events[j];

			if (event.description == nullptr) {
				continue;
			}

			const unsigned short *index = string_index.getptr(event.description);
			if (index == nullptr) {
				string_index[event.description] = next_index;
				++next_index;
			}
		}
	}

	FileAccess *f = FileAccess::open(_profiler_name + "_profiling_data.bin", FileAccess::WRITE);
	ERR_FAIL_COND(f == nullptr);

	f->store_16(string_index.size());

	{
		const char *const *key = NULL;
		while ((key = string_index.next(key))) {
			unsigned short p = string_index.get(*key);
			f->store_16(p);
			f->store_pascal_string(String(*key));
		}
	}

	//	for (auto it = string_index.begin(); it != string_index.end(); ++it) {
	//		f->store_16(it->second);
	//		f->store_string(String(it->first));
	//	}

	int page_count = _current_page + 1;
	if (page_count >= _pages.size()) {
		page_count = _pages.size();
	}

	for (int i = 0; i < page_count; ++i) {
		const Page &page = *_pages[i];

		for (int j = 0; j < page.write_index; ++j) {
			const Event &event = page.events[j];

			unsigned short desc_index = 0;
			if (event.description != nullptr) {
				desc_index = string_index.get(event.description);
			}

			f->store_32(event.time);
			f->store_8(event.type);
			f->store_16(desc_index);
		}
	}

	f->close();
	memdelete(f);
}

#endif // VOXEL_PROFILING
