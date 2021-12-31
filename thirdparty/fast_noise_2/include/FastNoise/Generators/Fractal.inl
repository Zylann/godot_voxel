#include "FastSIMD/InlInclude.h"

#include "Fractal.h"

template<typename FS, typename T>
class FS_T<FastNoise::Fractal<T>, FS> : public virtual FastNoise::Fractal<T>, public FS_T<FastNoise::Generator, FS>
{

};

template<typename FS>
class FS_T<FastNoise::FractalFBm, FS> : public virtual FastNoise::FractalFBm, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v gain = this->GetSourceValue( mGain  , seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );
        float32v noise = this->GetSourceValue( mSource, seed, pos... );

        float32v sum = noise * amp;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= FnUtils::Lerp( float32v( 1 ), (noise + float32v( 1 )) * float32v( 0.5f ), weightedStrength );
            amp *= gain;

            noise = this->GetSourceValue( mSource, seed, (pos *= lacunarity)... );
            sum += noise * amp;
        }

        return sum;
    }
};

template<typename FS>
class FS_T<FastNoise::FractalRidged, FS> : public virtual FastNoise::FractalRidged, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT(int32v seed, P... pos) const
    {
        float32v gain = this->GetSourceValue( mGain, seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );
        float32v noise = FS_Abs_f32( this->GetSourceValue( mSource, seed, pos... ) );

        float32v sum = (noise * float32v( -2 ) + float32v( 1 )) * amp;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= FnUtils::Lerp( float32v( 1 ), float32v( 1 ) - noise, weightedStrength );
            amp *= gain;

            noise = FS_Abs_f32( this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) );
            sum += (noise * float32v( -2 ) + float32v( 1 )) * amp;
        }

        return sum;
    }
};

template<typename FS>
class FS_T<FastNoise::FractalPingPong, FS> : public virtual FastNoise::FractalPingPong, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    static float32v PingPong( float32v t )
    {
        t -= FS_Round_f32( t * float32v( 0.5f ) ) * float32v( 2 );
        return FS_Select_f32( t < float32v( 1 ), t, float32v( 2 ) - t );
    }

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v gain = this->GetSourceValue( mGain  , seed, pos... );
        float32v weightedStrength = this->GetSourceValue( mWeightedStrength, seed, pos... );
        float32v pingPongStrength = this->GetSourceValue( mPingPongStrength, seed, pos... );
        float32v lacunarity( mLacunarity );
        float32v amp( mFractalBounding );
        float32v noise = PingPong( (this->GetSourceValue( mSource, seed, pos... ) + float32v( 1 )) * pingPongStrength );

        float32v sum = noise * amp;

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= FnUtils::Lerp( float32v( 1 ), (noise + float32v( 1 )) * float32v( 0.5f ), weightedStrength );
            amp *= gain;

            noise = PingPong( (this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) + float32v( 1 )) * pingPongStrength );
            sum += noise * amp;
        }

        return sum;
    }
};
