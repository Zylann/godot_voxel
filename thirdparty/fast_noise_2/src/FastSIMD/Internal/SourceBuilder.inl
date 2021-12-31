#pragma once
#include "FastSIMD/FastSIMD.h"

template<typename CLASS, typename FS>
class FS_T;

template<typename CLASS, FastSIMD::eLevel LEVEL>
CLASS* FastSIMD::ClassFactory( FastSIMD::MemoryAllocator allocator ) 
{
    if constexpr( ( CLASS::Supported_SIMD_Levels & LEVEL & FastSIMD::COMPILED_SIMD_LEVELS ) != 0 )
    {
        static_assert( std::is_base_of_v<CLASS, FS_T<CLASS, FS_SIMD_CLASS>> );

        if( allocator )
        {
            void* alloc = allocator( sizeof( FS_T<CLASS, FS_SIMD_CLASS> ), alignof( FS_T<CLASS, FS_SIMD_CLASS> ) );
            
            return new( alloc ) FS_T<CLASS, FS_SIMD_CLASS>;
        }

        return new FS_T<CLASS, FS_SIMD_CLASS>;        
    }
    return nullptr; 
}

#define FASTSIMD_BUILD_CLASS( CLASS ) \
template FASTSIMD_API CLASS* FastSIMD::ClassFactory<CLASS, FS_SIMD_CLASS::SIMD_Level>( FastSIMD::MemoryAllocator );

#include "../FastSIMD_BuildList.inl"
