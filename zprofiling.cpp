#include "zprofiling.h"

#ifdef VOXEL_PROFILING
#include <core/os/os.h>

//-----------------------------------------------------------------------------
class ZProfileVar {
public:
	static const unsigned int BUFFERED_TIME_COUNT = 100;

	ZProfileVar();

	void begin(uint64_t time);
	void end(uint64_t time);

	Dictionary serialize();

private:
	float min_time;
	float max_time;
	float total_time;
	float instant_time;
	int hits;
	float buffered_times[BUFFERED_TIME_COUNT];
	unsigned int buffered_time_index;

	float _begin_time;
};

//-----------------------------------------------------------------------------
inline uint64_t get_time() {
	return OS::get_singleton()->get_ticks_usec();
	//return OS::get_singleton()->get_ticks_msec();
}

ZProfileVar::ZProfileVar() {
	instant_time = 0.f;
	min_time = 0.f;
	max_time = 0.f;
	total_time = 0.f;
	hits = 0;
	_begin_time = 0.f;
	for (unsigned int i = 0; i < BUFFERED_TIME_COUNT; ++i)
		buffered_times[i] = 0;
	buffered_time_index = 0;
}

void ZProfileVar::begin(uint64_t time) {
	_begin_time = time;
}

void ZProfileVar::end(uint64_t time) {
	instant_time = time - _begin_time;

	if (hits == 0) {
		min_time = instant_time;
		max_time = instant_time;
	} else {
		if (instant_time < min_time)
			min_time = instant_time;

		if (instant_time > max_time)
			max_time = instant_time;
	}

	total_time += instant_time;

	buffered_times[buffered_time_index] = instant_time;
	++buffered_time_index;
	if (buffered_time_index >= BUFFERED_TIME_COUNT)
		buffered_time_index = 0;

	++hits;
}

Dictionary ZProfileVar::serialize() {
	Dictionary d;
	d["instant_time"] = instant_time;
	d["min_time"] = min_time;
	d["max_time"] = max_time;
	d["mean_time"] = hits > 0 ? total_time / static_cast<float>(hits) : 0;
	d["total_time"] = total_time;
	d["hits"] = hits;

	Array a;
	for (unsigned int i = 0; i < BUFFERED_TIME_COUNT; ++i) {
		a.append(buffered_times[i]);
	}
	d["buffered_times"] = a;
	d["buffered_time_index"] = buffered_time_index;

	return d;
}

//-----------------------------------------------------------------------------
//ZProfiler g_zprofiler;

//ZProfiler & ZProfiler::get() {
//	return g_zprofiler;
//}

ZProfiler::~ZProfiler() {
	const String *key = NULL;
	while (key = _vars.next(key)) {
		ZProfileVar *v = _vars.get(*key);
		memdelete(v);
	}
}

ZProfileVar *ZProfiler::get_var(String key) {
	ZProfileVar *v = NULL;
	ZProfileVar **pv = _vars.getptr(key);
	if (pv == NULL) {
		v = memnew(ZProfileVar);
		_vars[key] = v;
	} else {
		v = *pv;
	}
	return v;
}

void ZProfiler::begin(String key) {
	ZProfileVar *v = get_var(key);
	v->begin(get_time());
}

void ZProfiler::end(String key) {
	uint64_t time = get_time();
	ZProfileVar *v = get_var(key);
	v->end(time);
}

Dictionary ZProfiler::get_all_serialized_info() const {
	Dictionary d;
	const String *key = NULL;
	while (key = _vars.next(key)) {
		ZProfileVar *v = _vars.get(*key);
		d[*key] = v->serialize();
	}
	return d;
}

#endif // VOXEL_PROFILING
