#pragma once
#include "FastSIMD/FastSIMD.h"

#define FASTNOISE_CALC_MIN_MAX 1

namespace FastNoise
{
    const FastSIMD::Level_BitFlags SUPPORTED_SIMD_LEVELS =
        FastSIMD::Level_Scalar |
        FastSIMD::Level_SSE2   |
        FastSIMD::Level_SSE41  |
        FastSIMD::Level_AVX2   |
        FastSIMD::Level_AVX512 ;

    template<typename T = class Generator>
    using SmartNode = std::shared_ptr<T>;

    template<typename T = class Generator>
    using SmartNodeArg = const SmartNode<T>&;
}