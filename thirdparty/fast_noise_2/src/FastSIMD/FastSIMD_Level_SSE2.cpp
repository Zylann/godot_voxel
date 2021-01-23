#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SSE2
#include "Internal/SSE.h"
#define FS_SIMD_CLASS FastSIMD::SSE2
#include "Internal/SourceBuilder.inl"
#endif
