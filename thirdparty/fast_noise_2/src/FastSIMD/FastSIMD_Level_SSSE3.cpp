#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SSSE3
#include "Internal/SSE.h"
#define FS_SIMD_CLASS FastSIMD::SSSE3
#include "Internal/SourceBuilder.inl"
#endif
