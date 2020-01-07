#include "zprofiling.h"

#ifdef VOXEL_PROFILING
#include <core/os/os.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace {
thread_local ZProfiler g_profiler;
}

ZProfiler &ZProfiler::get_thread_profiler() {
	return g_profiler;
}

inline uint64_t get_time() {
	return OS::get_singleton()->get_ticks_usec();
}

ZProfiler::ZProfiler() {
	std::stringstream ss;
	ss << std::this_thread::get_id();
	_profiler_name = ss.str();
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

void ZProfiler::set_profiler_name(std::string name) {
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
				printf("ZProfiler end of capacity\n");
			}
		}
	}
}

void ZProfiler::dump() {

	printf("Dumping ZProfiler data\n");

	unsigned short next_index = 1;
	std::unordered_map<const char *, unsigned short> string_index;

	for (int i = 0; i < _pages.size(); ++i) {
		const Page &page = *_pages[i];

		for (int j = 0; j < page.events.size(); ++j) {
			const Event &event = page.events[j];

			if (event.description == nullptr) {
				continue;
			}

			auto it = string_index.find(event.description);
			if (it == string_index.end()) {
				string_index.insert(std::make_pair(event.description, next_index));
				++next_index;
			}
		}
	}

	std::ofstream ofs(_profiler_name + "_profiling_data.bin", std::ios::out | std::ios::binary | std::ios::trunc);
	ERR_FAIL_COND(!ofs.good());

	uint16_t string_index_size = string_index.size();
	ofs.write((char *)&string_index_size, sizeof(uint16_t));

	for (auto it = string_index.begin(); it != string_index.end(); ++it) {

		const char *key = it->first;
		uint16_t p = it->second;
		uint32_t len = std::strlen(key);

		ofs.write((char *)&p, sizeof(uint16_t));
		ofs.write((char *)&len, sizeof(uint32_t));
		ofs.write(key, len);
	}

	int page_count = _current_page + 1;
	if (page_count >= _pages.size()) {
		page_count = _pages.size();
	}

	int level = 0;
	uint32_t last_time = 0;

	for (int i = 0; i < page_count; ++i) {
		const Page &page = *_pages[i];

		for (int j = 0; j < page.write_index; ++j) {
			const Event &event = page.events[j];
			last_time = event.time;

			if (event.type == EVENT_PUSH) {
				++level;
			} else if (event.type == EVENT_POP) {
				if (level > 0) {
					--level;
				} else {
					printf("ZProfiler: unexpected pop\n");
					continue;
				}
			} else {
				CRASH_COND(true);
			}

			unsigned short desc_index = 0;
			if (event.description != nullptr) {
				desc_index = string_index[event.description];
			}

			ofs.write((char *)&event.time, sizeof(uint32_t));
			ofs.write((char *)&event.type, sizeof(uint8_t));
			ofs.write((char *)&desc_index, sizeof(uint16_t));
		}
	}

	while (level > 0) {
		printf("ZProfiler: filling missing pop\n");

		uint32_t time = last_time + 1;
		uint8_t type = EVENT_POP;
		uint16_t desc_index = 0;

		ofs.write((char *)&time, sizeof(uint32_t));
		ofs.write((char *)&type, sizeof(uint8_t));
		ofs.write((char *)&desc_index, sizeof(uint16_t));

		--level;
	}

	ofs.close();
}

#endif // VOXEL_PROFILING
