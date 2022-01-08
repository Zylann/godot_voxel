#pragma once

#ifdef __GNUG__
#include <x86intrin.h>
#else
#include <intrin.h>
#endif

#include "VecTools.h"

namespace FastSIMD
{
    struct AVX_f32x8
    {
        FASTSIMD_INTERNAL_TYPE_SET( AVX_f32x8, __m256 );

        FS_INLINE static AVX_f32x8 Incremented()
        {
            return _mm256_set_ps( 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f );
        }

        FS_INLINE explicit AVX_f32x8( float f )
        {
            *this = _mm256_set1_ps( f );
        }

        FS_INLINE explicit AVX_f32x8( float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7 )
        {
            *this = _mm256_set_ps( f7, f6, f5, f4, f3, f2, f1, f0 );
        }

        FS_INLINE AVX_f32x8& operator+=( const AVX_f32x8& rhs )
        {
            *this = _mm256_add_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator-=( const AVX_f32x8& rhs )
        {
            *this = _mm256_sub_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator*=( const AVX_f32x8& rhs )
        {
            *this = _mm256_mul_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator/=( const AVX_f32x8& rhs )
        {
            *this = _mm256_div_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator&=( const AVX_f32x8& rhs )
        {
            *this = _mm256_and_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator|=( const AVX_f32x8& rhs )
        {
            *this = _mm256_or_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8& operator^=( const AVX_f32x8& rhs )
        {
            *this = _mm256_xor_ps( *this, rhs );
            return *this;
        }

        FS_INLINE AVX_f32x8 operator~() const
        {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
            const __m256i neg1 = _mm256_cmpeq_epi32( _mm256_setzero_si256(), _mm256_setzero_si256() );
#else
            const __m256i neg1 = _mm256_set1_epi32( -1 );
#endif
            return _mm256_xor_ps( *this, _mm256_castsi256_ps( neg1 ) );
        }

        FS_INLINE AVX_f32x8 operator-() const
        {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
            const __m256i minInt = _mm256_slli_epi32( _mm256_cmpeq_epi32( _mm256_setzero_si256(), _mm256_setzero_si256() ), 31 );
#else
            const __m256i minInt = _mm256_set1_epi32( 0x80000000 );
#endif
            return _mm256_xor_ps( *this, _mm256_castsi256_ps( minInt ) );
        }

        FS_INLINE __m256i operator==( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_EQ_OS ) );
        }

        FS_INLINE __m256i operator!=( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_NEQ_OS ) );
        }

        FS_INLINE __m256i operator>( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_GT_OS ) );
        }

        FS_INLINE __m256i operator<( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_LT_OS ) );
        }

        FS_INLINE __m256i operator>=( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_GE_OS ) );
        }

        FS_INLINE __m256i operator<=( const AVX_f32x8& rhs )
        {
            return _mm256_castps_si256( _mm256_cmp_ps( *this, rhs, _CMP_LE_OS ) );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_FLOAT( AVX_f32x8 )


    struct AVX2_i32x8
    {
        FASTSIMD_INTERNAL_TYPE_SET( AVX2_i32x8, __m256i );

        FS_INLINE static AVX2_i32x8 Incremented()
        {
            return _mm256_set_epi32( 7, 6, 5, 4, 3, 2, 1, 0 );
        }

        FS_INLINE explicit AVX2_i32x8( int32_t f )
        {
            *this = _mm256_set1_epi32( f );
        }

        FS_INLINE explicit AVX2_i32x8( int32_t i0, int32_t i1, int32_t i2, int32_t i3, int32_t i4, int32_t i5, int32_t i6, int32_t i7 )
        {
            *this = _mm256_set_epi32( i7, i6, i5, i4, i3, i2, i1, i0 );
        }

        FS_INLINE AVX2_i32x8& operator+=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_add_epi32( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator-=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_sub_epi32( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator*=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_mullo_epi32( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator&=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_and_si256( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator|=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_or_si256( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator^=( const AVX2_i32x8& rhs )
        {
            *this = _mm256_xor_si256( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator>>=( int32_t rhs )
        {
            *this = _mm256_srai_epi32( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8& operator<<=( int32_t rhs )
        {
            *this = _mm256_slli_epi32( *this, rhs );
            return *this;
        }

        FS_INLINE AVX2_i32x8 operator~() const
        {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
            const __m256i neg1 = _mm256_cmpeq_epi32( _mm256_setzero_si256(), _mm256_setzero_si256() );
#else
            const __m256i neg1 = _mm256_set1_epi32( -1 );
#endif
            return _mm256_xor_si256( *this, neg1 );
        }

        FS_INLINE AVX2_i32x8 operator-() const
        {
            return _mm256_sub_epi32( _mm256_setzero_si256(), *this );
        }

        FS_INLINE AVX2_i32x8 operator==( const AVX2_i32x8& rhs )
        {
            return _mm256_cmpeq_epi32( *this, rhs );
        }

        FS_INLINE AVX2_i32x8 operator>( const AVX2_i32x8& rhs )
        {
            return _mm256_cmpgt_epi32( *this, rhs );
        }

        FS_INLINE AVX2_i32x8 operator<( const AVX2_i32x8& rhs )
        {
            return _mm256_cmpgt_epi32( rhs, *this );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_INT( AVX2_i32x8, int32_t )

    template<eLevel LEVEL_T>
    class AVX_T
    {
    public:
        static_assert( LEVEL_T >= Level_AVX && LEVEL_T <= Level_AVX2, "Cannot create template with unsupported SIMD level" );

        static constexpr eLevel SIMD_Level = LEVEL_T;

        template<size_t ElementSize>
        static constexpr size_t VectorSize = (256 / 8) / ElementSize;

        typedef AVX_f32x8  float32v;
        typedef AVX2_i32x8 int32v;
        typedef AVX2_i32x8 mask32v;

        // Load

        FS_INLINE static float32v Load_f32( void const* p )
        {
            return _mm256_loadu_ps( reinterpret_cast<float const*>(p) );
        }

        FS_INLINE static int32v Load_i32( void const* p )
        {
            return _mm256_loadu_si256( reinterpret_cast<__m256i const*>(p) );
        }

        // Store

        FS_INLINE static void Store_f32( void* p, float32v a )
        {
            _mm256_storeu_ps( reinterpret_cast<float*>(p), a );
        }

        FS_INLINE static void Store_i32( void* p, int32v a )
        {
            _mm256_storeu_si256( reinterpret_cast<__m256i*>(p), a );
        }

        // Extract

        FS_INLINE static float Extract0_f32( float32v a )
        {
            return _mm256_cvtss_f32( a );
        }

        FS_INLINE static int32_t Extract0_i32( int32v a )
        {
            return _mm_cvtsi128_si32(_mm256_castsi256_si128( a ));
        }

        FS_INLINE static float Extract_f32( float32v a, size_t idx )
        {
            float f[8];
            Store_f32( &f, a );
            return f[idx & 7];
        }

        FS_INLINE static int32_t Extract_i32( int32v a, size_t idx )
        {
            int32_t i[8];
            Store_i32( &i, a );
            return i[idx & 7];
        }

        // Cast

        FS_INLINE static float32v Casti32_f32( int32v a )
        {
            return _mm256_castsi256_ps( a );
        }

        FS_INLINE static int32v Castf32_i32( float32v a )
        {
            return _mm256_castps_si256( a );
        }

        // Convert

        FS_INLINE static float32v Converti32_f32( int32v a )
        {
            return _mm256_cvtepi32_ps( a );
        }

        FS_INLINE static int32v Convertf32_i32( float32v a )
        {
            return _mm256_cvtps_epi32( a );
        }

        // Select

        FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
        {
            return  _mm256_blendv_ps( b, a, _mm256_castsi256_ps( m ) );
        }

        FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
        {
            return _mm256_castps_si256( _mm256_blendv_ps( _mm256_castsi256_ps( b ), _mm256_castsi256_ps( a ), _mm256_castsi256_ps( m ) ) );
        }

        // Min, Max

        FS_INLINE static float32v Min_f32( float32v a, float32v b )
        {
            return _mm256_min_ps( a, b );
        }

        FS_INLINE static float32v Max_f32( float32v a, float32v b )
        {
            return _mm256_max_ps( a, b );
        }

        FS_INLINE static int32v Min_i32( int32v a, int32v b )
        {
            return _mm256_min_epi32( a, b );
        }

        FS_INLINE static int32v Max_i32( int32v a, int32v b )
        {
            return _mm256_max_epi32( a, b );
        }

        // Bitwise

        FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
        {
            return _mm256_andnot_ps( b, a );
        }

        FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
        {
            return _mm256_andnot_si256( b, a );
        }

        FS_INLINE static float32v BitwiseShiftRightZX_f32( float32v a, int32_t b )
        {
            return Casti32_f32( _mm256_srli_epi32( Castf32_i32( a ), b ) );
        }

        FS_INLINE static int32v BitwiseShiftRightZX_i32( int32v a, int32_t b )
        {
            return _mm256_srli_epi32( a, b );
        }

        // Abs

        FS_INLINE static float32v Abs_f32( float32v a )
        {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
            const __m256i intMax = _mm256_srli_epi32( _mm256_cmpeq_epi32( _mm256_setzero_si256(), _mm256_setzero_si256() ), 1 );
#else
            const __m256i intMax = _mm256_set1_epi32( 0x7FFFFFFF );
#endif
            return _mm256_and_ps( a, _mm256_castsi256_ps( intMax ) );
        }

        FS_INLINE static int32v Abs_i32( int32v a )
        {
            return _mm256_abs_epi32( a );
        }

        // Float math

        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return _mm256_sqrt_ps( a );
        }

        FS_INLINE static float32v InvSqrt_f32( float32v a )
        {
            return _mm256_rsqrt_ps( a );
        }

        FS_INLINE static float32v Reciprocal_f32( float32v a )
        {
            return _mm256_rcp_ps( a );
        }

        // Floor, Ceil, Round

        FS_INLINE static float32v Floor_f32( float32v a )
        {
            return _mm256_round_ps( a, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC );
        }

        FS_INLINE static float32v Ceil_f32( float32v a )
        {
            return _mm256_round_ps( a, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC );
        }

        FS_INLINE static float32v Round_f32( float32v a )
        {
            return _mm256_round_ps( a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC );
        }

        //Mask

        FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
        {
            return a & m;
        }

        FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
        {
            return _mm256_and_ps( a, _mm256_castsi256_ps( m ) );
        }

        FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
        {
            return _mm256_andnot_si256( m, a );
        }

        FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
        {
            return _mm256_andnot_ps( _mm256_castsi256_ps( m ), a );
        }

        FS_INLINE static bool AnyMask_bool( mask32v m )
        {
            return !_mm256_testz_si256( m, m );
        }
    };

#if FASTSIMD_COMPILE_AVX
    typedef AVX_T<Level_AVX>  AVX;
#endif

#if FASTSIMD_COMPILE_AVX2
    typedef AVX_T<Level_AVX2> AVX2;

#if FASTSIMD_USE_FMA
    template<>
    FS_INLINE AVX2::float32v FMulAdd_f32<AVX2>( AVX2::float32v a, AVX2::float32v b, AVX2::float32v c )
    {
        return _mm256_fmadd_ps( a, b, c );
    }

    template<>
    FS_INLINE AVX2::float32v FNMulAdd_f32<AVX2>( AVX2::float32v a, AVX2::float32v b, AVX2::float32v c )
    {
        return _mm256_fnmadd_ps( a, b, c );
    }
#endif
#endif
    
}
