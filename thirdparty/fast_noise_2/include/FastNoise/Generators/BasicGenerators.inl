#include "FastSIMD/InlInclude.h"

#include "BasicGenerators.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Constant, FS> : public virtual FastNoise::Constant, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return float32v( mValue );
    }
};

template<typename FS>
class FS_T<FastNoise::White, FS> : public virtual FastNoise::White, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos = FS_Casti32_f32( (FS_Castf32_i32( pos ) ^ (FS_Castf32_i32( pos ) >> 16)) * int32v( FnPrimes::Lookup[idx++] ) )), ...);

        return FnUtils::GetValueCoord( seed, FS_Castf32_i32( pos )... );
    }
};

template<typename FS>
class FS_T<FastNoise::Checkerboard, FS> : public virtual FastNoise::Checkerboard, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v multiplier = FS_Reciprocal_f32( float32v( mSize ) );

        int32v value = (FS_Convertf32_i32( pos * multiplier ) ^ ...);

        return float32v( 1.0f ) ^ FS_Casti32_f32( value << 31 );
    }
};

template<typename FS>
class FS_T<FastNoise::SineWave, FS> : public virtual FastNoise::SineWave, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v multiplier = FS_Reciprocal_f32( float32v( mScale ) );

        return (FS_Sin_f32( pos * multiplier ) * ...);
    }
};

template<typename FS>
class FS_T<FastNoise::PositionOutput, FS> : public virtual FastNoise::PositionOutput, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t offsetIdx = 0;
        size_t multiplierIdx = 0;

        (((pos += float32v( mOffset[offsetIdx++] )) *= float32v( mMultiplier[multiplierIdx++] )), ...);
        return (pos + ...);
    }
};

template<typename FS>
class FS_T<FastNoise::DistanceToPoint, FS> : public virtual FastNoise::DistanceToPoint, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t pointIdx = 0;

        ((pos -= float32v( mPoint[pointIdx++] )), ...);
        return FnUtils::CalcDistance( mDistanceFunction, pos... );
    }
};
