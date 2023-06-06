#pragma once


#include <arm_neon.h>

#include "VecTools.h"

#if defined(__arm__)
#define FASTSIMD_USE_ARMV7
#endif


namespace FastSIMD
{
    
    struct NEON_i32x4
    {
        FASTSIMD_INTERNAL_TYPE_SET( NEON_i32x4, int32x4_t );
        

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
            int32x4_t rhs2 = vdupq_n_s32( -rhs );
            *this = vshlq_s32(*this, rhs2);//use shift right by constant for faster execution
            return *this;
        }

        FS_INLINE NEON_i32x4& operator<<=( const int32_t rhs )
        {
            int32x4_t rhs2 = vdupq_n_s32( rhs );
            *this = vshlq_s32(*this, rhs2);//use shift left by constant for faster execution
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
        
        FS_INLINE NEON_i32x4 operator<( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcltq_s32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator>( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcgtq_s32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator<=( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcleq_s32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator>=( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcgeq_s32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator!=( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vmvnq_u32 (vceqq_s32( *this, b ) ) );
        }
        FS_INLINE NEON_i32x4 operator==( const NEON_i32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vceqq_s32( *this, b ) );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_INT( NEON_i32x4, int32_t )
    

    struct NEON_f32x4
    {
        FASTSIMD_INTERNAL_TYPE_SET( NEON_f32x4, float32x4_t );


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
        
        #ifdef FASTSIMD_USE_ARMV7
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
        #else
            FS_INLINE NEON_f32x4& operator/=( const NEON_f32x4& rhs )
            {
                /*
                float32x4_t reciprocal = vrecpeq_f32( rhs );
                reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );
                reciprocal = vmulq_f32( vrecpsq_f32( rhs, reciprocal ), reciprocal );
                *this = vmulq_f32( *this, reciprocal );
                */
                *this = vdivq_f32( *this, rhs );
                
                return *this;
            }
        #endif

        
        
        
        FS_INLINE NEON_f32x4& operator&=( const NEON_f32x4& rhs )
        {
            *this = vreinterpretq_f32_s32( vandq_s32( vreinterpretq_s32_f32( *this ), vreinterpretq_s32_f32( rhs ) ) );
            return *this;
        }
        FS_INLINE NEON_f32x4& operator|=( const NEON_f32x4& rhs )
        {
            *this = vreinterpretq_f32_s32( vorrq_s32( vreinterpretq_s32_f32( *this ), vreinterpretq_s32_f32( rhs ) ) );
            return *this;
        }
        FS_INLINE NEON_f32x4& operator^=( const NEON_f32x4& rhs )
        {
            *this = vreinterpretq_f32_s32( veorq_s32( vreinterpretq_s32_f32( *this ), vreinterpretq_s32_f32( rhs ) ) );
            return *this;
        }

        FS_INLINE NEON_f32x4 operator-() const
        {
            return vnegq_f32( *this );
        }
        FS_INLINE NEON_f32x4 operator~() const
        {
            return vreinterpretq_f32_u32( vmvnq_u32( vreinterpretq_u32_f32(*this) ) );
        }
        
        
        FS_INLINE NEON_i32x4 operator<( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcltq_f32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator>( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcgtq_f32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator<=( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcleq_f32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator>=( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vcgeq_f32( *this, b ) );
        }
        FS_INLINE NEON_i32x4 operator!=( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vmvnq_u32 (vceqq_f32( *this, b ) ) );
        }
        FS_INLINE NEON_i32x4 operator==( const NEON_f32x4 &b ) const
        {
            return vreinterpretq_s32_u32( vceqq_f32( *this, b ) );
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_FLOAT( NEON_f32x4 )


    
    

    template<eLevel LEVEL_T>
    class NEON_T
    {
    public:
        static constexpr eLevel SIMD_Level = FastSIMD::Level_NEON;

        
        template<size_t ElementSize>
        static constexpr size_t VectorSize = (128 / 8) / ElementSize;

        typedef NEON_f32x4 float32v;
        typedef NEON_i32x4   int32v;
        typedef NEON_i32x4  mask32v;

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
            return vcvtq_s32_f32( Round_f32(a) );
        }        

        // Comparisons

        FS_INLINE static mask32v Equal_f32( float32v a, float32v b )
        {
            return vreinterpretq_s32_u32( vceqq_f32( a, b ) );
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
            return vreinterpretq_s32_u32( vceqq_s32( a, b ) );
        }

        FS_INLINE static mask32v GreaterThan_i32( int32v a, int32v b )
        {
            return vreinterpretq_s32_u32( vcgtq_s32( a, b ) );
        }

        FS_INLINE static mask32v LessThan_i32( int32v a, int32v b )
        {
            return vreinterpretq_s32_u32( vcltq_s32( a, b ) );
        }

        // Select

        FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
        {
            return vbslq_f32( vreinterpretq_u32_s32( m ), a, b );
        }
        FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
        {
            return vbslq_s32( vreinterpretq_u32_s32( m ), a, b );
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
            return vreinterpretq_f32_u32( vandq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }
/*
        FS_INLINE static float32v BitwiseOr_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( vorrq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }

        FS_INLINE static float32v BitwiseXor_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( veorq_u32( vreinterpretq_u32_f32( a ), vreinterpretq_u32_f32( b ) ) );
        }

        FS_INLINE static float32v BitwiseNot_f32( float32v a )
        {
            return vreinterpretq_f32_u32( vmvnq_u32( vreinterpretq_u32_f32( a ) ) );
        }
*/
        FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
        {
            return vreinterpretq_f32_u32( vandq_u32( vreinterpretq_u32_f32( a ), vmvnq_u32( vreinterpretq_u32_f32( b ) ) ) );
        }

        FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
        {
            return vandq_s32( a , vmvnq_s32( b ) );
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

        FS_INLINE static float32v InvSqrt_f32( float32v a )
        {
            return vrsqrteq_f32( a );
        }        
        
        // Floor, Ceil, Round:

#ifdef FASTSIMD_USE_ARMV7    
        FS_INLINE static float32v IntFloor_f32(float32v a)
        {
            static const float32x4_t cmpval = vcvtq_f32_s32( vdupq_n_s32( 0x7FFFFFFF ) );            

            uint32x4_t cmp1 = vcagtq_f32( a, cmpval );
            uint32x4_t cmp2 = vcaleq_f32( a, cmpval );

            float32x4_t tr = vcvtq_f32_s32( vcvtq_s32_f32( a ) );

            uint32x4_t xcmp1 = vandq_u32(cmp1, vreinterpretq_u32_f32( a ) );
            uint32x4_t xcmp2 = vandq_u32(cmp2, vreinterpretq_u32_f32( tr ) );

            uint32x4_t res0 = vorrq_u32( xcmp1, xcmp2 );

            float32x4_t res1 = vreinterpretq_f32_u32( res0 );
            
            return res1;
        }
    
        FS_INLINE static float32v Floor_f32(float32v a)
        {
            static const float32x4_t zerox = vdupq_n_f32( 0 );

            float32x4_t ifl = IntFloor_f32(a);

            uint32x4_t cond1 = vmvnq_u32(vceqq_f32(a, ifl));
            uint32x4_t cond2 = vcltq_f32(a, zerox);

            uint32x4_t cmpmask = vandq_u32(cond1, cond2);
            float32x4_t addx = vcvtq_f32_s32( vreinterpretq_s32_u32(cmpmask) );

            float32x4_t ret0 = vaddq_f32(ifl, addx);

            return ret0;
        }

        FS_INLINE static float32v Ceil_f32(float32v a)
        {
            static const float32x4_t zerox = vdupq_n_f32( 0 );

            float32x4_t ifl = IntFloor_f32(a);
            
            uint32x4_t cond1 = vmvnq_u32(vceqq_f32(a, ifl));
            uint32x4_t cond2 = vcgeq_f32(a, zerox);

            uint32x4_t cmpmask = vandq_u32(cond1, cond2);
            float32x4_t addx = vcvtq_f32_s32( vreinterpretq_s32_u32(cmpmask) );
            

            float32x4_t ret0 = vsubq_f32(ifl, addx);

            return ret0;
        }

        FS_INLINE static float32v Round_f32(float32v a)
        {
            static const float32x4_t zerox = vdupq_n_f32( 0 );
            static const float32x4_t halfx = vdupq_n_f32( 0.5f );
            static const float32x4_t onex = vdupq_n_f32( 1.0f );
        
            float32x4_t a2 = vaddq_f32(vabsq_f32(a), halfx);
            float32x4_t ifl = IntFloor_f32(a2);          
            
            uint32x4_t cmpmask = vcltq_f32(a, zerox);
            float32x4_t rhs = vcvtq_f32_s32( vreinterpretq_s32_u32(cmpmask) );
            float32x4_t rhs2 = vaddq_f32(vmulq_n_f32(rhs, 2.0f), onex);            

            return vmulq_f32(ifl, rhs2);
        }
        
        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return Reciprocal_f32(InvSqrt_f32(a));
        }

    #else
        FS_INLINE static float32v Floor_f32( float32v a )
        {
            return vrndmq_f32( a );
        }

        FS_INLINE static float32v Ceil_f32( float32v a )
        {
            return vrndpq_f32( a );
        }

        FS_INLINE static float32v Round_f32( float32v a )
        {
            return vrndnq_f32( a );
        }

        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return vsqrtq_f32( a );
        }
    #endif      
        
        // Mask

        FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
        {
            return a & m;
        }

        FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
        {
            return BitwiseAndNot_i32(a, m);
        }

        FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
        {
            return BitwiseAnd_f32( a, vreinterpretq_f32_s32( m ) );
        }

        FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
        {
            return BitwiseAndNot_f32( a, vreinterpretq_f32_s32( m ) );
        }
        
        FS_INLINE static float Extract0_f32( float32v a )
        {
            return vgetq_lane_f32(a, 0);
        }

        FS_INLINE static int32_t Extract0_i32( int32v a )
        {
            return vgetq_lane_s32(a, 0);
        }

        FS_INLINE static float32v Reciprocal_f32( float32v a )
        {            
            return vrecpeq_f32( a );
        }

        FS_INLINE static float32v BitwiseShiftRightZX_f32( float32v a, int32_t b )
        {
            int32x4_t rhs2 = vdupq_n_s32( -b );
            return vreinterpretq_f32_u32 ( vshlq_u32( vreinterpretq_u32_f32(a), rhs2) );
        }
        
        FS_INLINE static int32v BitwiseShiftRightZX_i32( int32v a, int32_t b )
        {
            int32x4_t rhs2 = vdupq_n_s32( -b );
            return vreinterpretq_s32_u32 (vshlq_u32( vreinterpretq_u32_s32(a), rhs2));
        }
        FS_INLINE static bool AnyMask_bool( mask32v m )
        {
            uint32x2_t tmp = vorr_u32(vget_low_u32(vreinterpretq_u32_s32(m)), vget_high_u32(vreinterpretq_u32_s32(m)));
            return vget_lane_u32(vpmax_u32(tmp, tmp), 0);
        }
    };
    
#if FASTSIMD_COMPILE_NEON
    typedef NEON_T<Level_NEON> NEON;
#endif
}
