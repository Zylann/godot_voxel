#pragma once
#include <cinttypes>
#include <type_traits>
#include <memory>

#include "FastSIMD/FastSIMD.h"

#ifdef _MSC_VER
#if defined( _M_IX86_FP ) && _M_IX86_FP < 2
#define FS_VECTORCALL
#else
#define FS_VECTORCALL __vectorcall
#endif
#define FS_INLINE __forceinline
#else
#define FS_VECTORCALL 
#define FS_INLINE __attribute__((always_inline)) inline
#endif

#ifndef NDEBUG
#undef FS_INLINE
#define FS_INLINE inline
#endif

/// <summary>
/// Number of 32 width elements that will fit into a vector
/// </summary>
/// <remarks>
/// Compile time constant
/// </remarks>
/// <code>
/// size_t FS_Size_32()
/// </code>
#define FS_Size_32() FS::template VectorSize<sizeof( int32_t )>


// Vector builders

/// <summary>
/// Vector with values incrementing from 0 based on element index {0, 1, 2, 3...}
/// </summary>
/// <code>
/// example: int32v::FS_Incremented()
/// </code>
#define FS_Incremented() Incremented()


// Load

/// <summary>
/// Copies sizeof(float32v) bytes from given memory location into float32v
/// </summary>
/// <remarks>
/// Memory does not need to be aligned
/// </remarks>
/// <code>
/// float32v FS_Load_f32( void const* ptr )
/// </code>
#define FS_Load_f32( ... ) FS::Load_f32( __VA_ARGS__ )


/// <summary>
/// Copies sizeof(int32v) bytes from given memory location into int32v
/// </summary>
/// <remarks>
/// Memory does not need to be aligned
/// </remarks>
/// <code>
/// int32v FS_Load_i32( void const* ptr )
/// </code>
#define FS_Load_i32( ... ) FS::Load_i32( __VA_ARGS__ )


// Store

/// <summary>
/// Copies all elements of float32v to given memory location
/// </summary>
/// <code>
/// void FS_Store_f32( void* ptr, float32v f )
/// </code>
#define FS_Store_f32( ... ) FS::Store_f32( __VA_ARGS__ )

/// <summary>
/// Copies all elements of int32v to given memory location
/// </summary>
/// <code>
/// void FS_Store_i32( void* ptr, int32v i )
/// </code>
#define FS_Store_i32( ... ) FS::Store_i32( __VA_ARGS__ )


// Extract

/// <summary>
/// Retreive element 0 from vector
/// </summary>
/// <code>
/// float FS_Extract0_f32( float32v f )
/// </code>
#define FS_Extract0_f32( ... ) FS::Extract0_f32( __VA_ARGS__ )

/// <summary>
/// Retreive element 0 from vector
/// </summary>
/// <code>
/// int32_t FS_Extract0_i32( int32v i )
/// </code>
#define FS_Extract0_i32( ... ) FS::Extract0_i32( __VA_ARGS__ )

/// <summary>
/// Retreive element from vector at position
/// </summary>
/// <code>
/// float FS_Extract_f32( float32v f, size_t idx )
/// </code>
#define FS_Extract_f32( ... ) FS::Extract_f32( __VA_ARGS__ )

/// <summary>
/// Retreive element from vector at position
/// </summary>
/// <code>
/// int32_t FS_Extract_i32( int32v i, size_t idx )
/// </code>
#define FS_Extract_i32( ... ) FS::Extract_i32( __VA_ARGS__ )


// Cast

/// <summary>
/// Bitwise cast int to float
/// </summary>
/// <code>
/// float32v FS_Casti32_f32( int32v i )
/// </code>
#define FS_Casti32_f32( ... ) FS::Casti32_f32( __VA_ARGS__ )

/// <summary>
/// Bitwise cast float to int
/// </summary>
/// <code>
/// int32v FS_Castf32_i32( float32v f )
/// </code>
#define FS_Castf32_i32( ... ) FS::Castf32_i32( __VA_ARGS__ )


// Convert

/// <summary>
/// Convert int to float 
/// </summary>
/// <remarks>
/// Rounding: truncate
/// </remarks>
/// <code>
/// float32v FS_Converti32_f32( int32v i )
/// </code>
#define FS_Converti32_f32( ... ) FS::Converti32_f32( __VA_ARGS__ )

/// <summary>
/// Convert float to int
/// </summary>
/// <code>
/// int32v FS_Convertf32_i32( float32v f )
/// </code>
#define FS_Convertf32_i32( ... ) FS::Convertf32_i32( __VA_ARGS__ )


// Select

/// <summary>
/// return ( m ? a : b )
/// </summary>
/// <code>
/// float32v FS_Select_f32( mask32v m, float32v a, float32v b )
/// </code>
#define FS_Select_f32( ... ) FS::Select_f32( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : b )
/// </summary>
/// <code>
/// int32v FS_Select_i32( mask32v m, int32v a, int32v b )
/// </code>
#define FS_Select_i32( ... ) FS::Select_i32( __VA_ARGS__ )


// Min, Max

/// <summary>
/// return ( a < b ? a : b )
/// </summary>
/// <code>
/// float32v FS_Min_f32( float32v a, float32v b )
/// </code>
#define FS_Min_f32( ... ) FS::Min_f32( __VA_ARGS__ )

/// <summary>
/// return ( a > b ? a : b )
/// </summary>
/// <code>
/// float32v FS_Max_f32( float32v a, float32v b )
/// </code>
#define FS_Max_f32( ... ) FS::Max_f32( __VA_ARGS__ )

/// <summary>
/// return ( a < b ? a : b )
/// </summary>
/// <code>
/// int32v FS_Min_i32( int32v a, int32v b )
/// </code>
#define FS_Min_i32( ... ) FS::Min_i32( __VA_ARGS__ )

/// <summary>
/// return ( a > b ? a : b )
/// </summary>
/// <code>
/// int32v FS_Max_i32( int32v a, int32v b )
/// </code>
#define FS_Max_i32( ... ) FS::Max_i32( __VA_ARGS__ )


// Bitwise

/// <summary>
/// return ( a & ~b )
/// </summary>
/// <code>
/// float32v FS_BitwiseAndNot_f32( float32v a, float32v b )
/// </code>
#define FS_BitwiseAndNot_f32( ... ) FS::BitwiseAndNot_f32( __VA_ARGS__ )

/// <summary>
/// return ( a & ~b )
/// </summary>
/// <code>
/// int32v FS_BitwiseAndNot_i32( int32v a, int32v b )
/// </code>
#define FS_BitwiseAndNot_i32( ... ) FS::BitwiseAndNot_i32( __VA_ARGS__ )

/// <summary>
/// return ( a & ~b )
/// </summary>
/// <code>
/// mask32v FS_BitwiseAndNot_m32( mask32v a, mask32v b )
/// </code>
#define FS_BitwiseAndNot_m32( ... ) FastSIMD::BitwiseAndNot_m32<FS>( __VA_ARGS__ )


/// <summary>
/// return ZeroExtend( a >> b )
/// </summary>
/// <code>
/// float32v FS_BitwiseShiftRightZX_f32( float32v a, int32_t b )
/// </code>
#define FS_BitwiseShiftRightZX_f32( ... ) FS::BitwiseShiftRightZX_f32( __VA_ARGS__ )

/// <summary>
/// return ZeroExtend( a >> b )
/// </summary>
/// <code>
/// float32v FS_BitwiseShiftRightZX_i32( int32v a, int32_t b )
/// </code>
#define FS_BitwiseShiftRightZX_i32( ... ) FS::BitwiseShiftRightZX_i32( __VA_ARGS__ )

// Abs

/// <summary>
/// return ( a < 0 ? -a : a )
/// </summary>
/// <code>
/// float32v FS_Abs_f32( float32v a )
/// </code>
#define FS_Abs_f32( ... ) FS::Abs_f32( __VA_ARGS__ )

/// <summary>
/// return ( a < 0 ? -a : a )
/// </summary>
/// <code>
/// int32v FS_Abs_i32( int32v a )
/// </code>
#define FS_Abs_i32( ... ) FS::Abs_i32( __VA_ARGS__ )


// Float math

/// <summary>
/// return sqrt( a )
/// </summary>
/// <code>
/// float32v FS_Sqrt_f32( float32v a )
/// </code>
#define FS_Sqrt_f32( ... ) FS::Sqrt_f32( __VA_ARGS__ )

/// <summary>
/// return APPROXIMATE( 1.0 / sqrt( a ) )
/// </summary>
/// <code>
/// float32v FS_InvSqrt_f32( float32v a )
/// </code>
#define FS_InvSqrt_f32( ... ) FS::InvSqrt_f32( __VA_ARGS__ )

/// <summary>
/// return APPROXIMATE( 1.0 / a )
/// </summary>
/// <code>
/// float32v FS_Reciprocal_f32( float32v a )
/// </code>
#define FS_Reciprocal_f32( ... ) FS::Reciprocal_f32( __VA_ARGS__ )

// Floor, Ceil, Round

/// <summary>
/// return floor( a )
/// </summary>
/// <remarks>
/// Rounding: Towards negative infinity
/// </remarks>
/// <code>
/// float32v FS_Floor_f32( float32v a )
/// </code>
#define FS_Floor_f32( ... ) FS::Floor_f32( __VA_ARGS__ )

/// <summary>
/// return ceil( a )
/// </summary>
/// <remarks>
/// Rounding: Towards positive infinity
/// </remarks>
/// <code>
/// float32v FS_Ceil_f32( float32v a )
/// </code>
#define FS_Ceil_f32( ... ) FS::Ceil_f32( __VA_ARGS__ )

/// <summary>
/// return round( a )
/// </summary>
/// <remarks>
/// Rounding: Banker's rounding
/// </remarks>
/// <code>
/// float32v FS_Round_f32( float32v a )
/// </code>
#define FS_Round_f32( ... ) FS::Round_f32( __VA_ARGS__ )

// Trig

/// <summary>
/// return APPROXIMATE( cos( a ) )
/// </summary>
/// <code>
/// float32v FS_Cos_f32( float32v a )
/// </code>
#define FS_Cos_f32( ... ) FastSIMD::Cos_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return APPROXIMATE( sin( a ) )
/// </summary>
/// <code>
/// float32v FS_Sin_f32( float32v a )
/// </code>
#define FS_Sin_f32( ... ) FastSIMD::Sin_f32<FS>( __VA_ARGS__ )

// Math

/// <summary>
/// return pow( v, pow )
/// </summary>
/// <code>
/// float32v FS_Pow_f32( float32v v, float32v pow )
/// </code>
#define FS_Pow_f32( ... ) FastSIMD::Pow_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return log( a )
/// </summary>
/// <remarks>
/// a <= 0 returns 0
/// </remarks>
/// <code>
/// float32v FS_Log_f32( float32v a )
/// </code>
#define FS_Log_f32( ... ) FastSIMD::Log_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return exp( a )
/// </summary>
/// <remarks>
/// a will be clamped to -88.376, 88.376
/// </remarks>
/// <code>
/// float32v FS_Exp_f32( float32v a )
/// </code>
#define FS_Exp_f32( ... ) FastSIMD::Exp_f32<FS>( __VA_ARGS__ )


// Mask

/// <summary>
/// return ( m ? a : 0 )
/// </summary>
/// <code>
/// int32v FS_Mask_i32( int32v a, mask32v m )
/// </code>
#define FS_Mask_i32( ... ) FS::Mask_i32( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : 0 )
/// </summary>
/// <code>
/// float32v FS_Mask_f32( float32v a, mask32v m )
/// </code>
#define FS_Mask_f32( ... ) FS::Mask_f32( __VA_ARGS__ )

/// <summary>
/// return ( m ? 0 : a )
/// </summary>
/// <code>
/// int32v FS_NMask_i32( int32v a, mask32v m )
/// </code>
#define FS_NMask_i32( ... ) FS::NMask_i32( __VA_ARGS__ )

/// <summary>
/// return ( m ? 0 : a )
/// </summary>
/// <code>
/// float32v FS_NMask_f32( float32v a, mask32v m )
/// </code>
#define FS_NMask_f32( ... ) FS::NMask_f32( __VA_ARGS__ )

/// <summary>
/// return m.contains( true )
/// </summary>
/// <code>
/// bool FS_AnyMask_bool( mask32v m )
/// </code>
#define FS_AnyMask_bool( ... ) FS::AnyMask_bool( __VA_ARGS__ )


// FMA

/// <summary>
/// return ( (a * b) + c )
/// </summary>
/// <code>
/// float32v FS_FMulAdd_f32( float32v a, float32v b, float32v c )
/// </code>
#define FS_FMulAdd_f32( ... ) FastSIMD::FMulAdd_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( -(a * b) + c )
/// </summary>
/// <code>
/// float32v FS_FNMulAdd_f32( float32v a, float32v b, float32v c )
/// </code>
#define FS_FNMulAdd_f32( ... ) FastSIMD::FNMulAdd_f32<FS>( __VA_ARGS__ )


// Masked float

/// <summary>
/// return ( m ? (a + b) : a )
/// </summary>
/// <code>
/// float32v FS_MaskedAdd_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_MaskedAdd_f32( ... ) FastSIMD::MaskedAdd_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a - b) : a )
/// </summary>
/// <code>
/// float32v FS_MaskedSub_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_MaskedSub_f32( ... ) FastSIMD::MaskedSub_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a * b) : a )
/// </summary>
/// <code>
/// float32v FS_MaskedMul_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_MaskedMul_f32( ... ) FastSIMD::MaskedMul_f32<FS>( __VA_ARGS__ )


// Masked int32

/// <summary>
/// return ( m ? (a + b) : a )
/// </summary>
/// <code>
/// int32v FS_MaskedAdd_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_MaskedAdd_i32( ... ) FastSIMD::MaskedAdd_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a - b) : a )
/// </summary>
/// <code>
/// int32v FS_MaskedSub_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_MaskedSub_i32( ... ) FastSIMD::MaskedSub_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a * b) : a )
/// </summary>
/// <code>
/// int32v FS_MaskedMul_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_MaskedMul_i32( ... ) FastSIMD::MaskedMul_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a + 1) : a )
/// </summary>
/// <code>
/// int32v FS_MaskedIncrement_i32( int32v a, mask32v m )
/// </code>
#define FS_MaskedIncrement_i32( ... ) FastSIMD::MaskedIncrement_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? (a - 1) : a )
/// </summary>
/// <code>
/// int32v FS_MaskedDecrement_i32( int32v a, mask32v m )
/// </code>
#define FS_MaskedDecrement_i32( ... ) FastSIMD::MaskedDecrement_i32<FS>( __VA_ARGS__ )


// NMasked float

/// <summary>
/// return ( m ? a : (a + b) )
/// </summary>
/// <code>
/// float32v FS_NMaskedAdd_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_NMaskedAdd_f32( ... ) FastSIMD::NMaskedAdd_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : (a - b) )
/// </summary>
/// <code>
/// float32v FS_NMaskedSub_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_NMaskedSub_f32( ... ) FastSIMD::NMaskedSub_f32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : (a * b) )
/// </summary>
/// <code>
/// float32v FS_NMaskedMul_f32( float32v a, float32v b, mask32v m )
/// </code>
#define FS_NMaskedMul_f32( ... ) FastSIMD::NMaskedMul_f32<FS>( __VA_ARGS__ )


// NMasked int32

/// <summary>
/// return ( m ? a : (a + b) )
/// </summary>
/// <code>
/// int32v FS_NMaskedAdd_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_NMaskedAdd_i32( ... ) FastSIMD::NMaskedAdd_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : (a - b) )
/// </summary>
/// <code>
/// int32v FS_NMaskedSub_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_NMaskedSub_i32( ... ) FastSIMD::NMaskedSub_i32<FS>( __VA_ARGS__ )

/// <summary>
/// return ( m ? a : (a * b) )
/// </summary>
/// <code>
/// int32v FS_NMaskedMul_i32( int32v a, int32v b, mask32v m )
/// </code>
#define FS_NMaskedMul_i32( ... ) FastSIMD::NMaskedMul_i32<FS>( __VA_ARGS__ )


namespace FastSIMD
{
    //FMA

    template<typename FS>
    FS_INLINE typename FS::float32v FMulAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::float32v c )
    {
        return (a * b) + c;
    }

    template<typename FS>
    FS_INLINE typename FS::float32v FNMulAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::float32v c )
    {
        return -(a * b) + c;
    }

    // Masked float

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a + FS::Mask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedSub_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a - FS::Mask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v MaskedMul_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a * FS::Mask_f32( b, m );
    }

    // Masked int32

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedAdd_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a + FS::Mask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedSub_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a - FS::Mask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v MaskedMul_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a * FS::Mask_i32( b, m );
    }

    // NMasked float

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedAdd_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a + FS::NMask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedSub_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a - FS::NMask_f32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v NMaskedMul_f32( typename FS::float32v a, typename FS::float32v b, typename FS::mask32v m )
    {
        return a * FS::NMask_f32( b, m );
    }

    // NMasked int32

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedAdd_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a + FS::NMask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedSub_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a - FS::NMask_i32( b, m );
    }

    template<typename FS>
    FS_INLINE typename FS::int32v NMaskedMul_i32( typename FS::int32v a, typename FS::int32v b, typename FS::mask32v m )
    {
        return a * FS::NMask_i32( b, m );
    }

    template<typename FS, std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE typename FS::int32v MaskedIncrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return a - m;
    }

    template<typename FS, std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE typename FS::int32v MaskedIncrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return MaskedSub_i32<FS>( a, typename FS::int32v( -1 ), m );
    }
    template<typename FS, std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE typename FS::int32v MaskedDecrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return a + m;
    }

    template<typename FS, std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE typename FS::int32v MaskedDecrement_i32( typename FS::int32v a, typename FS::mask32v m )
    {
        return MaskedAdd_i32<FS>( a, typename FS::int32v( -1 ), m );
    }

    // Bitwise

    template<typename FS, std::enable_if_t<std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE  typename FS::mask32v BitwiseAndNot_m32( typename FS::mask32v a, typename FS::mask32v b )
    {
        return FS::BitwiseAndNot_i32( a, b );
    }

    template<typename FS, std::enable_if_t<!std::is_same_v<typename FS::int32v, typename FS::mask32v>>* = nullptr>
    FS_INLINE typename FS::mask32v BitwiseAndNot_m32( typename FS::mask32v a, typename FS::mask32v b )
    {
        return a & (~b);
    }

    // Trig

    template<typename FS>
    FS_INLINE typename FS::float32v Cos_f32( typename FS::float32v value )
    {
        typedef typename FS::int32v int32v;
        typedef typename FS::float32v float32v;
        typedef typename FS::mask32v mask32v;

        value = FS_Abs_f32( value );
        value -= FS_Floor_f32( value * float32v( 0.1591549f ) ) * float32v( 6.283185f );

        mask32v geHalfPi  = value >= float32v( 1.570796f );
        mask32v geHalfPi2 = value >= float32v( 3.141593f );
        mask32v geHalfPi3 = value >= float32v( 4.7123889f );

        float32v cosAngle = value ^ FS_Mask_f32( ( value ^ float32v( 3.141593f ) - value ), geHalfPi );
        cosAngle = cosAngle ^ FS_Mask_f32( FS_Casti32_f32( int32v( 0x80000000 ) ), geHalfPi2 );
        cosAngle = cosAngle ^ FS_Mask_f32( cosAngle ^ ( float32v( 6.283185f ) - value ), geHalfPi3 );

        cosAngle *= cosAngle;

        cosAngle = FS_FMulAdd_f32( cosAngle, FS_FMulAdd_f32( cosAngle, float32v( 0.03679168f ), float32v( -0.49558072f ) ), float32v( 0.99940307f ) );

        return cosAngle ^ FS_Mask_f32( FS_Casti32_f32( int32v( 0x80000000 ) ), FS_BitwiseAndNot_m32( geHalfPi, geHalfPi3 ) );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v Sin_f32( typename FS::float32v value )
    {
        return Cos_f32<FS>( typename FS::float32v( 1.570796f ) - value );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v Exp_f32( typename FS::float32v x )
    {
        typedef typename FS::int32v int32v;
        typedef typename FS::float32v float32v;

        x = FS_Min_f32( x, float32v( 88.3762626647949f ) );
        x = FS_Max_f32( x, float32v( -88.3762626647949f ) );

        /* express exp(x) as exp(g + n*log(2)) */
        float32v fx = x * float32v( 1.44269504088896341f );
        fx += float32v( 0.5f );

        float32v flr = FS_Floor_f32( fx );  
        fx = FS_MaskedSub_f32( flr, float32v( 1 ), flr > fx );

        x -= fx * float32v( 0.693359375f );
        x -= fx * float32v( -2.12194440e-4f );

        float32v y( 1.9875691500E-4f );
        y *= x;
        y += float32v( 1.3981999507E-3f );
        y *= x;
        y += float32v( 8.3334519073E-3f );
        y *= x;
        y += float32v( 4.1665795894E-2f );
        y *= x;
        y += float32v( 1.6666665459E-1f );
        y *= x;
        y += float32v( 5.0000001201E-1f );
        y *= x * x;
        y += x + float32v( 1 );        

        /* build 2^n */
        int32v i = FS_Convertf32_i32( fx );
        // another two AVX2 instructions
        i += int32v( 0x7f );
        i <<= 23;
        float32v pow2n = FS_Casti32_f32( i );
        
        return y * pow2n;        
    }

    template<typename FS>
    FS_INLINE typename FS::float32v Log_f32( typename FS::float32v x )
    {
        typedef typename FS::int32v int32v;
        typedef typename FS::float32v float32v;
        typedef typename FS::mask32v mask32v;
                
        mask32v validMask = x > float32v( 0 );

        x = FS_Max_f32( x, FS_Casti32_f32( int32v( 0x00800000 ) ) );  /* cut off denormalized stuff */

        // can be done with AVX2
        int32v i = FS_BitwiseShiftRightZX_i32( FS_Castf32_i32( x ), 23 );

        /* keep only the fractional part */
        x &= FS_Casti32_f32( int32v( ~0x7f800000 ) );
        x |= float32v( 0.5f );

        // this is again another AVX2 instruction
        i -= int32v( 0x7f );
        float32v e = FS_Converti32_f32( i );

        e += float32v( 1 );

        mask32v mask = x < float32v( 0.707106781186547524f );
        x = FS_MaskedAdd_f32( x, x, mask );
        x -= float32v( 1 );
        e = FS_MaskedSub_f32( e, float32v( 1 ), mask );

        float32v y = float32v( 7.0376836292E-2f );
        y *= x;
        y += float32v( -1.1514610310E-1f );
        y *= x;
        y += float32v( 1.1676998740E-1f );
        y *= x;
        y += float32v( -1.2420140846E-1f );
        y *= x;
        y += float32v( 1.4249322787E-1f );
        y *= x;
        y += float32v( -1.6668057665E-1f );
        y *= x;
        y += float32v( 2.0000714765E-1f );
        y *= x;
        y += float32v( -2.4999993993E-1f );
        y *= x;
        y += float32v( 3.3333331174E-1f );
        y *= x;

        float32v xx = x * x;
        y *= xx;
        y *= e * float32v( -2.12194440e-4f );
        y -= xx * float32v( 0.5f );

        x += y;
        x += e * float32v( 0.693359375f );

        return FS_Mask_f32( x, validMask );
    }

    template<typename FS>
    FS_INLINE typename FS::float32v Pow_f32( typename FS::float32v value, typename FS::float32v pow )
    {
        return Exp_f32<FS>( pow * Log_f32<FS>( value ) );
    }
}
