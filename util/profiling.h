#ifndef VOXEL_PROFILING_H
#define VOXEL_PROFILING_H

#if defined(TRACY_ENABLE)

#include <thirdparty/tracy/Tracy.hpp>
#include <thirdparty/tracy/common/TracySystem.hpp>

#define VOXEL_PROFILER_ENABLED

#define VOXEL_PROFILE_SCOPE() ZoneScoped
#define VOXEL_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)
#define VOXEL_PROFILE_MARK_FRAME() FrameMark
#define VOXEL_PROFILE_SET_THREAD_NAME(name) tracy::SetThreadName(name)
#define VOXEL_PROFILE_PLOT(name, number) TracyPlot(name, number)

#else

#define VOXEL_PROFILE_SCOPE()
// Name must be static const char* (usually string litteral)
#define VOXEL_PROFILE_SCOPE_NAMED(name)
#define VOXEL_PROFILE_MARK_FRAME()
#define VOXEL_PROFILE_PLOT(name, number)
// Name must be const char*. An internal copy will be made so it can be temporary.
#define VOXEL_PROFILE_SET_THREAD_NAME(name)

#endif

/*
To add Tracy support, clone it under thirdparty/tracy, and add the following lines in core/SCsub:

```
# tracy library
env.Append(CPPDEFINES="TRACY_ENABLE")
env_thirdparty.Append(CPPDEFINES="TRACY_ENABLE")
env_thirdparty.add_source_files(env.core_sources, ["#thirdparty/tracy/TracyClient.cpp"])
```
*/

#endif // VOXEL_PROFILING_H
