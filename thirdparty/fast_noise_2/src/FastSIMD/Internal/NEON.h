#pragma once

#include <arm_neon.h>

#include "VecTools.h"

struct NEON_f32x4
{
    FASTSIMD_INTERNAL_TYPE_SET( NEON_f32x4, float32x4_t );

    constexpr FS_INLINE static uint8_t Size()
    {
        return 4;
    }

    FS_INLINE static NEON_f32x4 Zero()
    {
        return vdupq_n_f32( 0 );
    }

    FS_INLINE static NEON_f32x4 Incremented()
    {
        alignas(16) const float f[4]{ 0.0f, 1.0f, 2.0f, 3.0f };
        return vld1q_f32( f );
    }

    FS_INLINE explicit NEON_f32x4( float f )
    {
        *this = vdupq_n_f32( f );
    }

    FS_INLINE explicit NEON_f32x4( float f0, float f1, float f2, float f3 )
    {
        alignas(16) const float f[4]{ f0, f1, f2, f3 };
        *this = vld1q_f32( f );
    }

    FS_INLINE NEON_f32x4& operator+=( const NEON_f32x4& rhs )
    {
        *this = vaddq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator-=( const NEON_f32x4& rhs )
    {
        *this = vsubq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator*=( const NEON_f32x4& rhs )
    {
        *this = vmulq_f32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_f32x4& operator/=( const NEON_f32x4& rhs )
    {
        float32x4_t reciprocal = vrecpeq_f32( rhs );
        // use a couple Newton-Raphson steps to refine the estimate.  Depending on your
        // application's accuracy requirements, you may be able to get away with only
        // one refinement (instead of the two used here).  Be sure to test!
        reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );
        reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );

        // and finally, compute a/b = a*(1/b)
        *this = vmulq_f32( *this, reciprocal );
        return *this;
    }

    FS_INLINE NEON_f32x4 operator-() const
    {
        return vnegq_f32( *this );
    }
};

FASTSIMD_INTERNAL_OPERATORS_FLOAT( NEON_f32x4 )


struct NEON_i32x4
{
    FASTSIMD_INTERNAL_TYPE_SET( NEON_i32x4, int32x4_t );

    constexpr FS_INLINE static uint8_t Size()
    {
        return 4;
    }

    FS_INLINE static NEON_i32x4 Zero()
    {
        return vdupq_n_s32( 0 );
    }

    FS_INLINE static NEON_i32x4 Incremented()
    {
        alignas(16) const int32_t f[4]{ 0, 1, 2, 3 };
        return vld1q_s32( f );
    }

    FS_INLINE explicit NEON_i32x4( int32_t i )
    {
        *this = vdupq_n_s32( i );
    }

    FS_INLINE explicit NEON_i32x4( int32_t i0, int32_t i1, int32_t i2, int32_t i3 )
    {
        alignas(16) const int32_t f[4]{ i0, i1, i2, i3 };
        *this = vld1q_s32( f );
    }

    FS_INLINE NEON_i32x4& operator+=( const NEON_i32x4& rhs )
    {
        *this = vaddq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator-=( const NEON_i32x4& rhs )
    {
        *this = vsubq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator*=( const NEON_i32x4& rhs )
    {
        *this = vmulq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator&=( const NEON_i32x4& rhs )
    {
        *this = vandq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator|=( const NEON_i32x4& rhs )
    {
        *this = vorrq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator^=( const NEON_i32x4& rhs )
    {
        *this = veorq_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator>>=( const int32_t rhs )
    {
        *this = vshrq_n_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4& operator<<=( const int32_t rhs )
    {
        *this = vshlq_n_s32( *this, rhs );
        return *this;
    }

    FS_INLINE NEON_i32x4 operator~() const
    {
        return vmvnq_s32( *this );
    }

    FS_INLINE NEON_i32x4 operator-() const
    {
        return vnegq_s32( *this );
    }
};

FASTSIMD_INTERNAL_OPERATORS_INT( NEON_i32x4, int32_t )

template<FastSIMD::eLevel LEVEL_T>
class FastSIMD_NEON_T
{
public:
    static const FastSIMD::eLevel SIMD_Level = LEVEL_T;
    static const size_t VectorSize = 128 / 8;

    typedef NEON_f32x4 float32v;
    typedef NEON_i32x4 int32v;
    typedef NEON_i32x4 mask32v;

    // Load

    FS_INLINE static float32v Load_f32( void const* p )
    {
        return vld1q_f32( reinterpret_cast<float const*>(p) );
    }

    FS_INLINE static int32v Load_i32( void const* p )
    {
        return vld1q_s32( reinterpret_cast<int32_t const*>(p) );
    }

    // Store

    FS_INLINE static void Store_f32( void* p, float32v a )
    {
        vst1q_f32( reinterpret_cast<float*>(p), a );
    }

    FS_INLINE static void Store_i32( void* p, int32v a )
    {
        vst1q_s32( reinterpret_cast<int32_t*>(p), a );
    }

    // Cast

    FS_INLINE static float32v Casti32_f32( int32v a )
    {
        return vreinterpretq_f32_s32( a );
    }

    FS_INLINE static int32v Castf32_i32( float32v a )
    {
        return vreinterpretq_s32_f32( a );
    }

    // Convert

    FS_INLINE static float32v Converti32_f32( int32v a )
    {
        return vcvtq_f32_s32( a );
    }

    FS_INLINE static int32v Convertf32_i32( float32v a )
    {
        return vcvtq_s32_f32( a );
    }

    // Comparisons

    FS_INLINE static mask32v Equal_f32( float32v a, float32v b )
    {
        return vreinterpretq_s32_u32( vceq_f32( a, b ) );
    }

    FS_INLINE static mask32v GreaterThan_f32( float32v a, float32v b )
    {
        return vreinterpretq_s32_u32( vcgtq_f32( a, b ) );
    }

    FS_INLINE static mask32v LessThan_f32( float32v a, float32v b )
    {
        return vreinterpretq_s32_u32( vcltq_f32( a, b ) );
    }

    FS_INLINE static mask32v GreaterEqualThan_f32( float32v a, float32v b )
    {
        return vreinterpretq_s32_u32( vcgeq_f32( a, b ) );
    }

    FS_INLINE static mask32v LessEqualThan_f32( float32v a, float32v b )
    {
        return vreinterpretq_s32_u32( vcleq_f32( a, b ) );
    }

    FS_INLINE static mask32v Equal_i32( int32v a, int32v b )
    {
        return vceq_s32( a, b );
    }

    FS_INLINE static mask32v GreaterThan_i32( int32v a, int32v b )
    {
        return vcgtq_s32( a, b );
    }

    FS_INLINE static mask32v LessThan_i32( int32v a, int32v b )
    {
        return vcltq_s32( a, b );
    }

    // Select

    FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
    {
        return vbslq_f32( vreinterpretq_u32_s32( mask ), b, a );
    }

    FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
    {
        return vbslq_s32( vreinterpretq_u32_s32( mask ), b, a );
    }

    // Min, Max

    FS_INLINE static float32v Min_f32( float32v a, float32v b )
    {
        return vminq_f32( a, b );
    }

    FS_INLINE static float32v Max_f32( float32v a, float32v b )
    {
        return vmaxq_f32( a, b );
    }

    FS_INLINE static int32v Min_i32( int32v a, int32v b )
    {
        return vminq_s32( a, b );
    }

    FS_INLINE static int32v Max_i32( int32v a, int32v b )
    {
        return vmaxq_s32( a, b );
    }
    
    // Bitwise

    FS_INLINE static float32v BitwiseAnd_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( vandq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseOr_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( vorrq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseXor_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( veorq_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseNot_f32( float32v a )
    {
        return vreinterpretq_f32_s32( vmvn_s32( vreinterpretq_s32_f32( a ), vreinterpretq_s32_f32( b ) ) );
    }

    FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
    {
        return vreinterpretq_f32_s32( vandq_s32( vreinterpretq_s32_f32( a ), vmvn_s32( vreinterpretq_s32_f32( b ) ) ) );
    }

    FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
    {
        return vandq_s32( a , vmvn_s32( b ) );
    }

    // Abs

    FS_INLINE static float32v Abs_f32( float32v a )
    {
        return vabsq_f32( a );
    }

    FS_INLINE static int32v Abs_i32( int32v a )
    {
        return vabsq_s32( a );
    }

    // Float math

    FS_INLINE static float32v Sqrt_f32( float32v a )
    {
        return vsqrtq_f32( a );
    }

    FS_INLINE static float32v InvSqrt_f32( float32v a )
    {
        return vrsqrteq_f32( a );
    }

    // Floor, Ceil, Round: http://dss.stephanierct.com/DevBlog/?p=8

    FS_INLINE static float32v Floor_f32( float32v a )
    {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
        const float32x4_t f1 = vdupq_n_f32( 1.0f ); //_mm_castsi128_ps( _mm_slli_epi32( _mm_srli_epi32( _mm_cmpeq_epi32( _mm_setzero_si128(), _mm_setzero_si128() ), 25 ), 23 ) );
#else
        const float32x4_t f1 = vdupq_n_f32( 1.0f );
#endif
        float32x4_t fval = vrndmq_f32( a );

        return vsubq_f32( fval, BitwiseAnd_f32( vcltq_f32( a, fval ), f1 ) );
    }

    FS_INLINE static float32v Ceil_f32( float32v a )
    {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
        const __m128 f1 = vdupq_n_f32( 1.0f ); //_mm_castsi128_ps( _mm_slli_epi32( _mm_srli_epi32( _mm_cmpeq_epi32( _mm_setzero_si128(), _mm_setzero_si128() ), 25 ), 23 ) );
#else
        const __m128 f1 = vdupq_n_f32( 1.0f );
#endif
        float32x4_t fval = vrndmq_f32( a );

        return vaddq_f32( fval, BitwiseAnd_f32( vcltq_f32( a, fval ), f1 ) );
    }

    template<FastSIMD::eLevel L = LEVEL_T>
    FS_INLINE static FS_ENABLE_IF( L < FastSIMD::ELevel_SSE41, float32v ) Round_f32( float32v a )
    {
#if FASTSIMD_CONFIG_GENERATE_CONSTANTS
        const __m128 nearest2 = _mm_castsi128_ps( _mm_srli_epi32( _mm_cmpeq_epi32( _mm_setzero_si128(), _mm_setzero_si128() ), 2 ) );
#else
        const __m128 nearest2 = vdupq_n_f32( 1.99999988079071044921875f );
#endif
        __m128 aTrunc = _mm_cvtepi32_ps( _mm_cvttps_epi32( a ) );       // truncate a
        __m128 rmd = _mm_sub_ps( a, aTrunc );                           // get remainder
        __m128 rmd2 = _mm_mul_ps( rmd, nearest2 );                      // mul remainder by near 2 will yield the needed offset
        __m128 rmd2Trunc = _mm_cvtepi32_ps( _mm_cvttps_epi32( rmd2 ) ); // after being truncated of course
        return _mm_add_ps( aTrunc, rmd2Trunc );
    }

    template<FastSIMD::eLevel L = LEVEL_T>
    FS_INLINE static FS_ENABLE_IF( L >= FastSIMD::ELevel_SSE41, float32v ) Round_f32( float32v a )
    {
        return vrndnq_f32( a );
    }

    // Mask

    FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
    {
        return a & m;
    }

    FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
    {
        return BitwiseAnd_f32( a, vreinterpretq_f32_s32( m ) );
    }
};

#if FASTSIMD_COMPILE_NEON
typedef FastSIMD_SSE_T<FastSIMD::ELevel_NEON> FastSIMD_NEON;
#endif
