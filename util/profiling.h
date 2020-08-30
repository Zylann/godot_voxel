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

#endif // VOXEL_PROFILING_H
