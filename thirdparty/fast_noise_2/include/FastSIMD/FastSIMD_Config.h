#pragma once
#include <cstdint>
#include <cstddef>

#include "FastSIMD_Export.h"

#if defined(__arm__) || defined(__aarch64__)
#define FASTSIMD_x86 false
#define FASTSIMD_ARM true
#else
#define FASTSIMD_x86 true
#define FASTSIMD_ARM false
#endif

#define FASTSIMD_64BIT (INTPTR_MAX == INT64_MAX)

#define FASTSIMD_COMPILE_SCALAR (!(FASTSIMD_x86 && FASTSIMD_64BIT)) // Don't compile for x86 64bit since CPU is guaranteed SSE2 support 

#define FASTSIMD_COMPILE_SSE    (FASTSIMD_x86 & false) // Not supported
#define FASTSIMD_COMPILE_SSE2   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE3   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSSE3  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE41  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_SSE42  (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_AVX    (FASTSIMD_x86 & false) // Not supported
#define FASTSIMD_COMPILE_AVX2   (FASTSIMD_x86 & true )
#define FASTSIMD_COMPILE_AVX512 (FASTSIMD_x86 & true )

#define FASTSIMD_COMPILE_NEON   (FASTSIMD_ARM & true )

#define FASTSIMD_USE_FMA                   true
#define FASTSIMD_CONFIG_GENERATE_CONSTANTS false

