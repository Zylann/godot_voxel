#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SSE41
#include "Internal/SSE.h"
#define FS_SIMD_CLASS FastSIMD::SSE41
#include "Internal/SourceBuilder.inl"
#endif
