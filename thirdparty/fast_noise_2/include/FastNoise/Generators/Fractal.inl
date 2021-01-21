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
        float32v sum  = this->GetSourceValue( mSource, seed, pos... );

        float32v lacunarity( mLacunarity );
        float32v amp( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= gain;
            sum += this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) * amp;
        }

        return sum * float32v( mFractalBounding );
    }
};

template<typename FS>
class FS_T<FastNoise::FractalBillow, FS> : public virtual FastNoise::FractalBillow, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v sum = FS_Abs_f32( this->GetSourceValue( mSource, seed, pos... ) ) * float32v( 2 ) - float32v( 1 );
        float32v gain = this->GetSourceValue( mGain, seed, pos... );

        float32v lacunarity( mLacunarity );
        float32v amp( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= gain;
            sum += (FS_Abs_f32(this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) ) * float32v( 2 ) - float32v( 1 )) * amp;
        }

        return sum * float32v( mFractalBounding );
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
        float32v sum = float32v( 1 ) - FS_Abs_f32( this->GetSourceValue( mSource, seed, pos... ) );
        float32v gain = this->GetSourceValue( mGain, seed, pos... );

        float32v lacunarity( mLacunarity );
        float32v amp( 1 );

        for( int i = 1; i < mOctaves; i++ )
        {
            seed -= int32v( -1 );
            amp *= gain;
            sum -= (float32v( 1 ) - FS_Abs_f32( this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ) )) * amp;
        }

        return sum;
    }
};

template<typename FS>
class FS_T<FastNoise::FractalRidgedMulti, FS> : public virtual FastNoise::FractalRidgedMulti, public FS_T<FastNoise::Fractal<>, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v offset( 1 );
        float32v sum = offset - FS_Abs_f32( this->GetSourceValue( mSource, seed, pos... ) );
        float32v gain = this->GetSourceValue( mGain, seed, pos... ) * float32v( 6 );
        
        float32v lacunarity( mLacunarity );
        float32v amp = sum;

        float32v weightAmp( mWeightAmp );
        float32v weight = weightAmp;
        float32v totalWeight( 1.0f );

        for( int i = 1; i < mOctaves; i++ )
        {
            amp *= gain;
            amp = FS_Min_f32( FS_Max_f32( amp, float32v( 0 ) ), float32v( 1 ) );

            seed -= int32v( -1 );
            float32v value = offset - FS_Abs_f32( this->GetSourceValue( mSource, seed, (pos *= lacunarity)... ));

            value *= amp;
            amp = value;

            float32v weightRecip = FS_Reciprocal_f32( float32v( weight ) );
            sum += value * weightRecip;
            totalWeight += weightRecip;
            weight *= weightAmp;
        }

        return sum * float32v( mWeightBounding ) - offset;
    }
};
