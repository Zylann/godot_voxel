#include "FastSIMD/FastSIMD.h"

#if FASTSIMD_COMPILE_SCALAR
#include "Internal/Scalar.h"
#define FS_SIMD_CLASS FastSIMD::Scalar
#include "Internal/SourceBuilder.inl"
#endif