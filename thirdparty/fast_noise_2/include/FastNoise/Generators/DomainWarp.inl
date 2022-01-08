#include "FastSIMD/InlInclude.h"

#include "DomainWarp.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::DomainWarp, FS> : public virtual FastNoise::DomainWarp, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        Warp( seed, this->GetSourceValue( mWarpAmplitude, seed, pos... ), (pos * float32v( mWarpFrequency ))..., pos... );

        return this->GetSourceValue( mSource, seed, pos...);
    }

public:
    float GetWarpFrequency() const { return mWarpFrequency; }
    const FastNoise::HybridSource& GetWarpAmplitude() const { return mWarpAmplitude; }
    const FastNoise::GeneratorSource& GetWarpSource() const { return mSource; }

    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const = 0;
    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const = 0;
    virtual float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const = 0;
};

template<typename FS>
class FS_T<FastNoise::DomainWarpGradient, FS> : public virtual FastNoise::DomainWarpGradient, public FS_T<FastNoise::DomainWarp, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

public:
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );

    #define GRADIENT_COORD( _x, _y )\
        int32v hash##_x##_y = FnUtils::HashPrimesHB(seed, x##_x, y##_y );\
        float32v x##_x##_y = FS_Converti32_f32( hash##_x##_y & int32v( 0xffff ) );\
        float32v y##_x##_y = FS_Converti32_f32( (hash##_x##_y >> 16) & int32v( 0xffff ) );

        GRADIENT_COORD( 0, 0 );
        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        float32v normalise = float32v( 1.0f / (0xffff / 2.0f) );

        float32v xWarp = (FnUtils::Lerp( FnUtils::Lerp( x00, x10, xs ), FnUtils::Lerp( x01, x11, xs ), ys ) - float32v( 0xffff / 2.0f )) * normalise;
        float32v yWarp = (FnUtils::Lerp( FnUtils::Lerp( y00, y10, xs ), FnUtils::Lerp( y01, y11, xs ), ys ) - float32v( 0xffff / 2.0f )) * normalise;

        xOut = FS_FMulAdd_f32( xWarp, warpAmp, xOut );
        yOut = FS_FMulAdd_f32( yWarp, warpAmp, yOut );

        float32v warpLengthSq = FS_FMulAdd_f32( xWarp, xWarp, yWarp * yWarp );

        return warpLengthSq * FS_InvSqrt_f32( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( FnPrimes::Z );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );
        int32v z1 = z0 + int32v( FnPrimes::Z );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );
        zs = FnUtils::InterpHermite( z - zs );

    #define GRADIENT_COORD( _x, _y, _z )\
        int32v hash##_x##_y##_z = FnUtils::HashPrimesHB( seed, x##_x, y##_y, z##_z );\
        float32v x##_x##_y##_z = FS_Converti32_f32( hash##_x##_y##_z & int32v( 0x3ff ) );\
        float32v y##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 10) & int32v( 0x3ff ) );\
        float32v z##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 20) & int32v( 0x3ff ) );

        GRADIENT_COORD( 0, 0, 0 );
        GRADIENT_COORD( 1, 0, 0 );
        GRADIENT_COORD( 0, 1, 0 );
        GRADIENT_COORD( 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 1 );
        GRADIENT_COORD( 0, 1, 1 );
        GRADIENT_COORD( 1, 1, 1 );

    #undef GRADIENT_COORD

        float32v x0z = FnUtils::Lerp( FnUtils::Lerp( x000, x100, xs ), FnUtils::Lerp( x010, x110, xs ), ys );
        float32v y0z = FnUtils::Lerp( FnUtils::Lerp( y000, y100, xs ), FnUtils::Lerp( y010, y110, xs ), ys );
        float32v z0z = FnUtils::Lerp( FnUtils::Lerp( z000, z100, xs ), FnUtils::Lerp( z010, z110, xs ), ys );
                   
        float32v x1z = FnUtils::Lerp( FnUtils::Lerp( x001, x101, xs ), FnUtils::Lerp( x011, x111, xs ), ys );
        float32v y1z = FnUtils::Lerp( FnUtils::Lerp( y001, y101, xs ), FnUtils::Lerp( y011, y111, xs ), ys );
        float32v z1z = FnUtils::Lerp( FnUtils::Lerp( z001, z101, xs ), FnUtils::Lerp( z011, z111, xs ), ys );

        float32v normalise = float32v( 1.0f / (0x3ff / 2.0f) );

        float32v xWarp = (FnUtils::Lerp( x0z, x1z, zs ) - float32v( 0x3ff / 2.0f )) * normalise;
        float32v yWarp = (FnUtils::Lerp( y0z, y1z, zs ) - float32v( 0x3ff / 2.0f )) * normalise;
        float32v zWarp = (FnUtils::Lerp( z0z, z1z, zs ) - float32v( 0x3ff / 2.0f )) * normalise;

        xOut = FS_FMulAdd_f32( xWarp, warpAmp, xOut );
        yOut = FS_FMulAdd_f32( yWarp, warpAmp, yOut );
        zOut = FS_FMulAdd_f32( zWarp, warpAmp, zOut );

        float32v warpLengthSq = FS_FMulAdd_f32( xWarp, xWarp, FS_FMulAdd_f32( yWarp, yWarp, zWarp * zWarp ) );

        return warpLengthSq * FS_InvSqrt_f32( warpLengthSq );
    }
            
    float32v FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );
        float32v ws = FS_Floor_f32( w );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( FnPrimes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( FnPrimes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( FnPrimes::Z );
        int32v w0 = FS_Convertf32_i32( ws ) * int32v( FnPrimes::W );
        int32v x1 = x0 + int32v( FnPrimes::X );
        int32v y1 = y0 + int32v( FnPrimes::Y );
        int32v z1 = z0 + int32v( FnPrimes::Z );
        int32v w1 = w0 + int32v( FnPrimes::W );

        xs = FnUtils::InterpHermite( x - xs );
        ys = FnUtils::InterpHermite( y - ys );
        zs = FnUtils::InterpHermite( z - zs );
        ws = FnUtils::InterpHermite( w - ws );

    #define GRADIENT_COORD( _x, _y, _z, _w )\
        int32v hash##_x##_y##_z##_w = FnUtils::HashPrimesHB( seed, x##_x, y##_y, z##_z, w##_w );\
        float32v x##_x##_y##_z##_w = FS_Converti32_f32( hash##_x##_y##_z##_w & int32v( 0xff ) );\
        float32v y##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 8) & int32v( 0xff ) );\
        float32v z##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 16) & int32v( 0xff ) );\
        float32v w##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 24) & int32v( 0xff ) );

        GRADIENT_COORD( 0, 0, 0, 0 );
        GRADIENT_COORD( 1, 0, 0, 0 );
        GRADIENT_COORD( 0, 1, 0, 0 );
        GRADIENT_COORD( 1, 1, 0, 0 );
        GRADIENT_COORD( 0, 0, 1, 0 );
        GRADIENT_COORD( 1, 0, 1, 0 );
        GRADIENT_COORD( 0, 1, 1, 0 );
        GRADIENT_COORD( 1, 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 0, 1 );
        GRADIENT_COORD( 0, 1, 0, 1 );
        GRADIENT_COORD( 1, 1, 0, 1 );
        GRADIENT_COORD( 0, 0, 1, 1 );
        GRADIENT_COORD( 1, 0, 1, 1 );
        GRADIENT_COORD( 0, 1, 1, 1 );
        GRADIENT_COORD( 1, 1, 1, 1 );

    #undef GRADIENT_COORD

        float32v x0w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( x0000, x1000, xs ), FnUtils::Lerp( x0100, x1100, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( x0010, x1010, xs ), FnUtils::Lerp( x0110, x1110, xs ), ys ), zs );
        float32v y0w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( y0000, y1000, xs ), FnUtils::Lerp( y0100, y1100, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( y0010, y1010, xs ), FnUtils::Lerp( y0110, y1110, xs ), ys ), zs );
        float32v z0w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( z0000, z1000, xs ), FnUtils::Lerp( z0100, z1100, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( z0010, z1010, xs ), FnUtils::Lerp( z0110, z1110, xs ), ys ), zs );
        float32v w0w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( w0000, w1000, xs ), FnUtils::Lerp( w0100, w1100, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( w0010, w1010, xs ), FnUtils::Lerp( w0110, w1110, xs ), ys ), zs );

        float32v x1w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( x0001, x1001, xs ), FnUtils::Lerp( x0101, x1101, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( x0011, x1011, xs ), FnUtils::Lerp( x0111, x1111, xs ), ys ), zs );
        float32v y1w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( y0001, y1001, xs ), FnUtils::Lerp( y0101, y1101, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( y0011, y1011, xs ), FnUtils::Lerp( y0111, y1111, xs ), ys ), zs );
        float32v z1w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( z0001, z1001, xs ), FnUtils::Lerp( z0101, z1101, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( z0011, z1011, xs ), FnUtils::Lerp( z0111, z1111, xs ), ys ), zs );
        float32v w1w = FnUtils::Lerp( FnUtils::Lerp( FnUtils::Lerp( w0001, w1001, xs ), FnUtils::Lerp( w0101, w1101, xs ), ys ), FnUtils::Lerp( FnUtils::Lerp( w0011, w1011, xs ), FnUtils::Lerp( w0111, w1111, xs ), ys ), zs );                        

        float32v normalise = float32v( 1.0f / (0xff / 2.0f) );

        float32v xWarp = (FnUtils::Lerp( x0w, x1w, ws ) - float32v( 0xff / 2.0f )) * normalise;
        float32v yWarp = (FnUtils::Lerp( y0w, y1w, ws ) - float32v( 0xff / 2.0f )) * normalise;
        float32v zWarp = (FnUtils::Lerp( z0w, z1w, ws ) - float32v( 0xff / 2.0f )) * normalise;
        float32v wWarp = (FnUtils::Lerp( w0w, w1w, ws ) - float32v( 0xff / 2.0f )) * normalise;

        xOut = FS_FMulAdd_f32( xWarp, warpAmp, xOut );
        yOut = FS_FMulAdd_f32( yWarp, warpAmp, yOut );
        zOut = FS_FMulAdd_f32( zWarp, warpAmp, zOut );
        wOut = FS_FMulAdd_f32( wWarp, warpAmp, wOut );

        float32v warpLengthSq = FS_FMulAdd_f32( xWarp, xWarp, FS_FMulAdd_f32( yWarp, yWarp, FS_FMulAdd_f32( zWarp, zWarp, wWarp * wWarp ) ) );

        return warpLengthSq * FS_InvSqrt_f32( warpLengthSq );
    }
};

