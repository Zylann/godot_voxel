#ifndef VOXEL_PROFILING_H
#define VOXEL_PROFILING_H

#define VOXEL_PROFILER_ENABLED

#if defined(VOXEL_PROFILER_ENABLED) && defined(TRACY_ENABLE)

#include <thirdparty/tracy/Tracy.hpp>

#define VOXEL_PROFILE_SCOPE() ZoneScoped
#define VOXEL_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)
#define VOXEL_PROFILE_MARK_FRAME() FrameMark

#else

#define VOXEL_PROFILE_SCOPE()
#define VOXEL_PROFILE_SCOPE_NAMED(name)
#define VOXEL_PROFILE_MARK_FRAME()

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
