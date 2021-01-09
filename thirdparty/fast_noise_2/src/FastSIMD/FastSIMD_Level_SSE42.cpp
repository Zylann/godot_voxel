#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SSE42
#include "Internal/SSE.h"
#define FS_SIMD_CLASS FastSIMD::SSE42
#include "Internal/SourceBuilder.inl"
#endif
