#pragma once
#include <FastSIMD/FastSIMD.h>
#include "FastNoise_Export.h"

#define FASTNOISE_CALC_MIN_MAX true
#define FASTNOISE_USE_SHARED_PTR false

#if FASTNOISE_USE_SHARED_PTR
#include <memory>
#endif

namespace FastNoise
{
    const FastSIMD::Level_BitFlags SUPPORTED_SIMD_LEVELS =
        FastSIMD::Level_Scalar |
        FastSIMD::Level_SSE2   |
        FastSIMD::Level_SSE41  |
        FastSIMD::Level_AVX2   |
        FastSIMD::Level_AVX512 |
        FastSIMD::Level_NEON   ;
    
    class Generator;
    struct Metadata;

    template<typename T>
    struct MetadataT;

#if FASTNOISE_USE_SHARED_PTR
    template<typename T = Generator>
    using SmartNode = std::shared_ptr<T>;
#else
    template<typename T = Generator>
    class SmartNode;
#endif

    template<typename T = Generator>
    using SmartNodeArg = const SmartNode<const T>&;

    template<typename T>
    SmartNode<T> New( FastSIMD::eLevel maxSimdLevel = FastSIMD::Level_Null );
} // namespace FastNoise

#if !FASTNOISE_USE_SHARED_PTR
#include "SmartNode.h"
#endif
