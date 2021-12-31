#pragma once
#include "FastSIMD_Config.h"

namespace FastSIMD
{
    typedef uint32_t Level_BitFlags;

    enum eLevel : Level_BitFlags
    {
        Level_Null   = 0,       // Uninitilised
        Level_Scalar = 1 <<  0, // 80386 instruction set (Not SIMD)
        Level_SSE    = 1 <<  1, // SSE (XMM) supported by CPU (not testing for O.S. support)
        Level_SSE2   = 1 <<  2, // SSE2
        Level_SSE3   = 1 <<  3, // SSE3
        Level_SSSE3  = 1 <<  4, // Supplementary SSE3 (SSSE3)
        Level_SSE41  = 1 <<  5, // SSE4.1
        Level_SSE42  = 1 <<  6, // SSE4.2
        Level_AVX    = 1 <<  7, // AVX supported by CPU and operating system
        Level_AVX2   = 1 <<  8, // AVX2
        Level_AVX512 = 1 <<  9, // AVX512, AVX512DQ supported by CPU and operating system

        Level_NEON   = 1 << 16, // ARM NEON
    };

    const Level_BitFlags COMPILED_SIMD_LEVELS =
        (FASTSIMD_COMPILE_SCALAR     ? Level_Scalar : 0) |
        (FASTSIMD_COMPILE_SSE        ? Level_SSE    : 0) |
        (FASTSIMD_COMPILE_SSE2       ? Level_SSE2   : 0) |
        (FASTSIMD_COMPILE_SSE3       ? Level_SSE3   : 0) |
        (FASTSIMD_COMPILE_SSSE3      ? Level_SSSE3  : 0) |
        (FASTSIMD_COMPILE_SSE41      ? Level_SSE41  : 0) |
        (FASTSIMD_COMPILE_SSE42      ? Level_SSE42  : 0) |
        (FASTSIMD_COMPILE_AVX        ? Level_AVX    : 0) |
        (FASTSIMD_COMPILE_AVX2       ? Level_AVX2   : 0) |
        (FASTSIMD_COMPILE_AVX512     ? Level_AVX512 : 0) |
        (FASTSIMD_COMPILE_NEON       ? Level_NEON   : 0) ;
    
    typedef void* (*MemoryAllocator)( size_t size, size_t align );

    FASTSIMD_API eLevel CPUMaxSIMDLevel();

    template<typename T>
    T* New( eLevel maxSIMDLevel = Level_Null, MemoryAllocator allocator = nullptr );

    template<typename T, eLevel SIMD_LEVEL>
    T* ClassFactory( MemoryAllocator allocator = nullptr );

#define FASTSIMD_LEVEL_SUPPORT( ... ) \
    static const FastSIMD::Level_BitFlags Supported_SIMD_Levels = __VA_ARGS__

}
