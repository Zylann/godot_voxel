#include "FS_Class.inl"
#ifdef FASTSIMD_INCLUDE_CHECK
#include __FILE__
#endif
#include "FS_Class.inl"
#pragma once

FASTSIMD_CLASS_DECLARATION( Example )
{
    FASTSIMD_CLASS_SETUP( FastSIMD::Level_AVX2 | FastSIMD::Level_SSE41 | FastSIMD::Level_SSE2 | FastSIMD::Level_Scalar );

public:

    FS_EXTERNAL_FUNC( void DoStuff( int* data ) );

    FS_EXTERNAL_FUNC( void DoArray( int* data0, int* data1, int size ) );
};
