#pragma once
#include "FastSIMD/FastSIMD.h"
#include "FastSIMD/TypeList.h"

template<typename CLASS, typename FS>
class FS_T;

template<typename CLASS, FastSIMD::eLevel LEVEL>
CLASS* FastSIMD::ClassFactory() 
{
    if constexpr( ( CLASS::Supported_SIMD_Levels & LEVEL & FastSIMD::COMPILED_SIMD_LEVELS ) != 0 )
    {
        static_assert( std::is_base_of_v<CLASS, FS_T<CLASS, FS_SIMD_CLASS>> );
        return new FS_T<CLASS, FS_SIMD_CLASS>; 
    }
    return nullptr; 
}

#define FASTSIMD_BUILD_CLASS( CLASS ) \
template CLASS* FastSIMD::ClassFactory<CLASS, FS_SIMD_CLASS::SIMD_Level>();

#include "../FastSIMD_BuildList.inl"
