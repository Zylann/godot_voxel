#pragma once

#if defined(__arm__) || defined(__aarch64__)
#define FASTSIMD_x86 0
#define FASTSIMD_ARM 1
#else
#define FASTSIMD_x86 1
#define FASTSIMD_ARM 0
#endif

#define FASTSIMD_64BIT (INTPTR_MAX == INT64_MAX)

#define FASTSIMD_COMPILE_SCALAR (!(FASTSIMD_x86 && FASTSIMD_64BIT)) // Don't compile for x86 64bit since CPU is guaranteed SSE2 support 

#define FASTSIMD_COMPILE_SSE    (FASTSIMD_x86 & 000) // Not supported
#define FASTSIMD_COMPILE_SSE2   (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_SSE3   (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_SSSE3  (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_SSE41  (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_SSE42  (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_AVX    (FASTSIMD_x86 & 000) // Not supported
#define FASTSIMD_COMPILE_AVX2   (FASTSIMD_x86 &  1 )
#define FASTSIMD_COMPILE_AVX512 (FASTSIMD_x86 &  1 )

#define FASTSIMD_COMPILE_NEON   (FASTSIMD_ARM &  1 )

#define FASTSIMD_USE_FMA                   1
#define FASTSIMD_CONFIG_GENERATE_CONSTANTS 0

