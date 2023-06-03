#pragma once

#include "VecTools.h"
#include <algorithm>
#include <cmath>

namespace FastSIMD
{
    template<typename OUT, typename IN>
    OUT ScalarCast( IN a )
    {
        union
        {
            OUT o;
            IN  i;
        } u;

        u.i = a;
        return u.o;
    }

    struct Scalar_Float
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Float, float );

        FS_INLINE static Scalar_Float Incremented()
        {
            return 0.0f;
        }

        FS_INLINE Scalar_Float& operator+=( const Scalar_Float& rhs )
        {
            vector += rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator-=( const Scalar_Float& rhs )
        {
            vector -= rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator*=( const Scalar_Float& rhs )
        {
            vector *= rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator/=( const Scalar_Float& rhs )
        {
            vector /= rhs;
            return *this;
        }

        FS_INLINE Scalar_Float& operator&=( const Scalar_Float& rhs )
        {
            *this = ScalarCast<float>( ScalarCast<int32_t, float>( *this ) & ScalarCast<int32_t, float>( rhs ) );
            return *this;
        }

        FS_INLINE Scalar_Float& operator|=( const Scalar_Float& rhs )
        {
            *this = ScalarCast<float>( ScalarCast<int32_t, float>( *this ) | ScalarCast<int32_t, float>( rhs ) );
            return *this;
        }

        FS_INLINE Scalar_Float& operator^=( const Scalar_Float& rhs )
        {
            *this = ScalarCast<float>( ScalarCast<int32_t, float>( *this ) ^ ScalarCast<int32_t, float>( rhs ) );
            return *this;
        }

        FS_INLINE Scalar_Float operator~() const
        {
            return ScalarCast<float>( ~ScalarCast<int32_t, float>( *this ) );
        }

        FS_INLINE Scalar_Float operator-() const
        {
            return -vector;
        }

        FS_INLINE bool operator==( const Scalar_Float& rhs )
        {
            return vector == rhs;
        }

        FS_INLINE bool operator!=( const Scalar_Float& rhs )
        {
            return vector != rhs;
        }

        FS_INLINE bool operator>( const Scalar_Float& rhs )
        {
            return vector > rhs;
        }

        FS_INLINE bool operator<( const Scalar_Float& rhs )
        {
            return vector < rhs;
        }

        FS_INLINE bool operator>=( const Scalar_Float& rhs )
        {
            return vector >= rhs;
        }

        FS_INLINE bool operator<=( const Scalar_Float& rhs )
        {
            return vector <= rhs;
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_FLOAT( Scalar_Float )


    struct Scalar_Int
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Int, int32_t );

        FS_INLINE static Scalar_Int Incremented()
        {
            return 0;
        }

        FS_INLINE Scalar_Int& operator+=( const Scalar_Int& rhs )
        {
            vector += rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator-=( const Scalar_Int& rhs )
        {
            vector -= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator*=( const Scalar_Int& rhs )
        {
            vector *= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator&=( const Scalar_Int& rhs )
        {
            vector &= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator|=( const Scalar_Int& rhs )
        {
            vector |= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator^=( const Scalar_Int& rhs )
        {
            vector ^= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator>>=( int32_t rhs )
        {
            vector >>= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int& operator<<=( int32_t rhs )
        {
            vector <<= rhs;
            return *this;
        }

        FS_INLINE Scalar_Int operator~() const
        {
            return ~vector;
        }

        FS_INLINE Scalar_Int operator-() const
        {
            return -vector;
        }

        FS_INLINE bool operator==( const Scalar_Int& rhs )
        {
            return vector == rhs;
        }

        FS_INLINE bool operator>( const Scalar_Int& rhs )
        {
            return vector > rhs;
        }

        FS_INLINE bool operator<( const Scalar_Int& rhs )
        {
            return vector < rhs;
        }
    };

    FASTSIMD_INTERNAL_OPERATORS_INT( Scalar_Int, int32_t )


    struct Scalar_Mask
    {
        FASTSIMD_INTERNAL_TYPE_SET( Scalar_Mask, bool );

        FS_INLINE Scalar_Mask operator~() const
        {
            return !vector;
        }

        FS_INLINE Scalar_Mask& operator&=( const Scalar_Mask& rhs )
        {
            vector = vector && rhs;
            return *this;
        }

        FS_INLINE Scalar_Mask& operator|=( const Scalar_Mask& rhs )
        {
            vector = vector || rhs;
            return *this;
        }

        FS_INLINE Scalar_Mask operator&( const Scalar_Mask& rhs )
        {
            return vector && rhs;
        }

        FS_INLINE Scalar_Mask operator|( const Scalar_Mask& rhs )
        {
            return vector || rhs;
        }
    };

    class Scalar
    {
    public:
        static constexpr eLevel SIMD_Level = FastSIMD::Level_Scalar;

        template<size_t ElementSize = 8>
        static constexpr size_t VectorSize = sizeof(int32_t) / ElementSize;

        typedef Scalar_Float float32v;
        typedef Scalar_Int   int32v;
        typedef Scalar_Mask  mask32v;

        // Load

        FS_INLINE static float32v Load_f32( void const* p )
        {
            return *reinterpret_cast<float32v const*>(p);
        }

        FS_INLINE static int32v Load_i32( void const* p )
        {
            return *reinterpret_cast<int32v const*>(p);
        }

        // Store

        FS_INLINE static void Store_f32( void* p, float32v a )
        {
            *reinterpret_cast<float32v*>(p) = a;
        }

        FS_INLINE static void Store_i32( void* p, int32v a )
        {
            *reinterpret_cast<int32v*>(p) = a;
        }

        // Extract

        FS_INLINE static float Extract0_f32( float32v a )
        {
            return a;
        }

        FS_INLINE static int32_t Extract0_i32( int32v a )
        {
            return a;
        }

        FS_INLINE static float Extract_f32( float32v a, size_t idx )
        {
            return a;
        }

        FS_INLINE static int32_t Extract_i32( int32v a, size_t idx )
        {
            return a;
        }

        // Cast

        FS_INLINE static float32v Casti32_f32( int32v a )
        {
            return ScalarCast<float, int32_t>( a );
        }

        FS_INLINE static int32v Castf32_i32( float32v a )
        {
            return ScalarCast<int32_t, float>( a );
        }

        // Convert

        FS_INLINE static float32v Converti32_f32( int32v a )
        {
            return static_cast<float>(a);
        }

        FS_INLINE static int32v Convertf32_i32( float32v a )
        {
            return static_cast<int32_t>(rintf( a ));
        }

        // Select

        FS_INLINE static float32v Select_f32( mask32v m, float32v a, float32v b )
        {
            return m ? a : b;
        }

        FS_INLINE static int32v Select_i32( mask32v m, int32v a, int32v b )
        {
            return m ? a : b;
        }

        // Min, Max

        FS_INLINE static float32v Min_f32( float32v a, float32v b )
        {
            return fminf( a, b );
        }

        FS_INLINE static float32v Max_f32( float32v a, float32v b )
        {
            return fmaxf( a, b );
        }

        FS_INLINE static int32v Min_i32( int32v a, int32v b )
        {
            return std::min( a, b );
        }

        FS_INLINE static int32v Max_i32( int32v a, int32v b )
        {
            return std::max( a, b );
        }

        // Bitwise       

        FS_INLINE static float32v BitwiseAndNot_f32( float32v a, float32v b )
        {
            return Casti32_f32( Castf32_i32( a ) & ~Castf32_i32( b ) );
        }

        FS_INLINE static int32v BitwiseAndNot_i32( int32v a, int32v b )
        {
            return a & ~b;
        }

        FS_INLINE static float32v BitwiseShiftRightZX_f32( float32v a, int32_t b )
        {
            return Casti32_f32( int32_t( uint32_t( Castf32_i32( a ) ) >> b ) );
        }

        FS_INLINE static int32v BitwiseShiftRightZX_i32( int32v a, int32_t b )
        {
            return int32_t( uint32_t( a ) >> b );
        }

        // Abs

        FS_INLINE static float32v Abs_f32( float32v a )
        {
            return fabsf( a );
        }

        FS_INLINE static int32v Abs_i32( int32v a )
        {
            return abs( a );
        }

        // Float math

        FS_INLINE static float32v Sqrt_f32( float32v a )
        {
            return sqrtf( a );
        }

        FS_INLINE static float32v InvSqrt_f32( float32v a )
        {
            float xhalf = 0.5f * (float)a;
            a = Casti32_f32( 0x5f3759df - ((int32_t)Castf32_i32( a ) >> 1) );
            a *= (1.5f - xhalf * (float)a * (float)a);
            return a;
        }

        FS_INLINE static float32v Reciprocal_f32( float32v a )
        {
            // pow( pow(x,-0.5), 2 ) = pow( x, -1 ) = 1.0 / x
            a = Casti32_f32( (0xbe6eb3beU - (int32_t)Castf32_i32( a )) >> 1 );
            return a * a;
        }

        // Floor, Ceil, Round

        FS_INLINE static float32v Floor_f32( float32v a )
        {
            return floorf( a );
        }

        FS_INLINE static float32v Ceil_f32( float32v a )
        {
            return ceilf( a );
        }

        FS_INLINE static float32v Round_f32( float32v a )
        {
            return nearbyintf( a );
        }

        // Mask

        FS_INLINE static int32v Mask_i32( int32v a, mask32v m )
        {
            return m ? a : int32v(0);
        }

        FS_INLINE static float32v Mask_f32( float32v a, mask32v m )
        {
            return m ? a : float32v(0);
        }

        FS_INLINE static int32v NMask_i32( int32v a, mask32v m )
        {
            return m ? int32v(0) : a;
        }

        FS_INLINE static float32v NMask_f32( float32v a, mask32v m )
        {
            return m ? float32v(0) : a;
        }

        FS_INLINE static bool AnyMask_bool( mask32v m )
        {
            return m;
        }
    };
}
