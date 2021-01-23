#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SSE3
#include "Internal/SSE.h"
#define FS_SIMD_CLASS FastSIMD::SSE3
#include "Internal/SourceBuilder.inl"
#endif
