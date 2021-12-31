#include "FastSIMD/InlInclude.h"

#include "Blends.h"

template<typename FS>
class FS_T<FastNoise::Add, FS> : public virtual FastNoise::Add, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) + this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::Subtract, FS> : public virtual FastNoise::Subtract, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) - this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::Multiply, FS> : public virtual FastNoise::Multiply, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) * this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::Divide, FS> : public virtual FastNoise::Divide, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mLHS, seed, pos... ) / this->GetSourceValue( mRHS, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::PowFloat, FS> : public virtual FastNoise::PowFloat, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS_Pow_f32( this->GetSourceValue( mValue, seed, pos... ), this->GetSourceValue( mPow, seed, pos... ) );
    }
};

template<typename FS>
class FS_T<FastNoise::PowInt, FS> : public virtual FastNoise::PowInt, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v value = this->GetSourceValue( mValue, seed, pos... );
        float32v pow = value * value;

        for( int i = 2; i < mPow; i++ )
        {
            pow *= value;
        }

        return pow;
    }
};

template<typename FS>
class FS_T<FastNoise::Min, FS> : public virtual FastNoise::Min, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS_Min_f32( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<typename FS>
class FS_T<FastNoise::Max, FS> : public virtual FastNoise::Max, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return FS_Max_f32( this->GetSourceValue( mLHS, seed, pos... ), this->GetSourceValue( mRHS, seed, pos... ) );
    }
};

template<typename FS>
class FS_T<FastNoise::MinSmooth, FS> : public virtual FastNoise::MinSmooth, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = this->GetSourceValue( mLHS, seed, pos... );
        float32v b = this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS_Max_f32( float32v( 1.175494351e-38f ), FS_Abs_f32( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS_Max_f32( smoothness - FS_Abs_f32( a - b ), float32v( 0.0f ) );

        h *= FS_Reciprocal_f32( smoothness );

        return FS_FNMulAdd_f32( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS_Min_f32( a, b ) );
    }
};

template<typename FS>
class FS_T<FastNoise::MaxSmooth, FS> : public virtual FastNoise::MaxSmooth, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v a = -this->GetSourceValue( mLHS, seed, pos... );
        float32v b = -this->GetSourceValue( mRHS, seed, pos... );
        float32v smoothness = FS_Max_f32( float32v( 1.175494351e-38f ), FS_Abs_f32( this->GetSourceValue( mSmoothness, seed, pos... ) ) );

        float32v h = FS_Max_f32( smoothness - FS_Abs_f32( a - b ), float32v( 0.0f ) );

        h *= FS_Reciprocal_f32( smoothness );

        return -FS_FNMulAdd_f32( float32v( 1.0f / 6.0f ), h * h * h * smoothness, FS_Min_f32( a, b ) );
    }
};

template<typename FS>
class FS_T<FastNoise::Fade, FS> : public virtual FastNoise::Fade, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v fade = FS_Abs_f32( this->GetSourceValue( mFade, seed, pos... ) );

        return FS_FMulAdd_f32( this->GetSourceValue( mA, seed, pos... ), float32v( 1 ) - fade, this->GetSourceValue( mB, seed, pos... ) * fade );
    }
};

