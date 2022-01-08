#pragma once

#include "FastSIMD.h"

namespace FastSIMD
{
    template<eLevel... T>
    struct SIMDTypeContainer
    {
        static constexpr eLevel MinimumCompiled = Level_Null;

        template<eLevel L>
        static constexpr eLevel GetNextCompiledAfter = Level_Null;
    };

    template<eLevel HEAD, eLevel... TAIL>
    struct SIMDTypeContainer<HEAD, TAIL...>
    {
        static constexpr eLevel MinimumCompiled = (HEAD & COMPILED_SIMD_LEVELS) != 0 ? HEAD : SIMDTypeContainer<TAIL...>::MinimumCompiled;

        template<eLevel L>
        static constexpr eLevel GetNextCompiledAfter = (L == HEAD) ? SIMDTypeContainer<TAIL...>::MinimumCompiled : SIMDTypeContainer<TAIL...>::template GetNextCompiledAfter<L>;
    };

    using SIMDTypeList = SIMDTypeContainer<
        Level_Scalar,
        Level_SSE,
        Level_SSE2,
        Level_SSE3,
        Level_SSSE3,
        Level_SSE41,
        Level_SSE42,
        Level_AVX,
        Level_AVX2,
        Level_AVX512,
        Level_NEON>;
}