#ifndef VOXEL_PROFILING_H
#define VOXEL_PROFILING_H

#define VOXEL_PROFILING

#ifdef VOXEL_PROFILING

#include <dictionary.h>
#include <hash_map.h>
#include <ustring.h>

#define VOXEL_PROFILE_BEGIN(_key) _zprofiler.begin(_key);
#define VOXEL_PROFILE_END(_key) _zprofiler.end(_key);

class ZProfileVar;

class ZProfiler {
public:
	//static ZProfiler & get();
	~ZProfiler();

	void begin(String key);
	void end(String key);

	Dictionary get_all_serialized_info() const;

private:
	//ZProfiler();
	ZProfileVar *get_var(String key);

	HashMap<String, ZProfileVar *> _vars;
};

#else

#define VOXEL_PROFILE_BEGIN(_key) //
#define VOXEL_PROFILE_END(_key) //

#endif

#endif // VOXEL_PROFILING_H
