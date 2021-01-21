#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_NEON
#include "Internal/NEON.h"
#define FS_SIMD_CLASS FastSIMD::NEON
#include "Internal/SourceBuilder.inl"
#endif