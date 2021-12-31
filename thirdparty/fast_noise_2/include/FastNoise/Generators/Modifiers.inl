#include "FastSIMD/InlInclude.h"

#include "Modifiers.h"

template<typename FS>
class FS_T<FastNoise::DomainScale, FS> : public virtual FastNoise::DomainScale, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed, (pos * float32v( mScale ))... );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainOffset, FS> : public virtual FastNoise::DomainOffset, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;
    
    template<typename... P> 
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return [this, seed]( std::remove_reference_t<P>... sourcePos, std::remove_reference_t<P>... offset )
        {
            size_t idx = 0;
            ((offset += this->GetSourceValue( mOffset[idx++], seed, sourcePos... )), ...);

            return this->GetSourceValue( mSource, seed, offset... );
        } (pos..., pos...);
    }
};

template<typename FS>
class FS_T<FastNoise::DomainRotate, FS> : public virtual FastNoise::DomainRotate, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        if( mPitchSin == 0.0f && mRollSin == 0.0f )
        {
            return this->GetSourceValue( mSource, seed,
                FS_FNMulAdd_f32( y, float32v( mYawSin ), x * float32v( mYawCos ) ),
                FS_FMulAdd_f32( x, float32v( mYawSin ), y * float32v( mYawCos ) ) );
        }

        return Gen( seed, x, y, float32v( 0 ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        return this->GetSourceValue( mSource, seed,
            FS_FMulAdd_f32( x, float32v( mXa ), FS_FMulAdd_f32( y, float32v( mXb ), z * float32v( mXc ) ) ),
            FS_FMulAdd_f32( x, float32v( mYa ), FS_FMulAdd_f32( y, float32v( mYb ), z * float32v( mYc ) ) ),
            FS_FMulAdd_f32( x, float32v( mZa ), FS_FMulAdd_f32( y, float32v( mZb ), z * float32v( mZc ) ) ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        // No rotation for 4D yet
        return this->GetSourceValue( mSource, seed, x, y, z, w );
    }
};

template<typename FS>
class FS_T<FastNoise::SeedOffset, FS> : public virtual FastNoise::SeedOffset, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        return this->GetSourceValue( mSource, seed + int32v( mOffset ), pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::Remap, FS> : public virtual FastNoise::Remap, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
            
        return float32v( mToMin ) + (( source - float32v( mFromMin ) ) / float32v( mFromMax - mFromMin ) * float32v( mToMax - mToMin ));
    }
};

template<typename FS>
class FS_T<FastNoise::ConvertRGBA8, FS> : public virtual FastNoise::ConvertRGBA8, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );
        
        source = FS_Min_f32( source, float32v( mMax ));
        source = FS_Max_f32( source, float32v( mMin ));
        source -= float32v( mMin );

        source *= float32v( 255.0f / (mMax - mMin) );

        int32v byteVal = FS_Convertf32_i32( source );

        int32v output = int32v( 255 << 24 );
        output |= byteVal;
        output |= byteVal << 8;
        output |= byteVal << 16;

        return FS_Casti32_f32( output );
    }
};

template<typename FS>
class FS_T<FastNoise::Terrace, FS> : public virtual FastNoise::Terrace, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        float32v source = this->GetSourceValue( mSource, seed, pos... );

        source *= float32v( mMultiplier );
        float32v rounded = FS_Round_f32( source );

        if( mSmoothness != 0.0f )
        {
            float32v diff = rounded - source;
            mask32v diffSign = diff < float32v( 0 );

            diff = FS_Abs_f32( diff );
            diff = float32v( 0.5f ) - diff;

            diff *= float32v( mSmoothnessRecip );
            diff = FS_Min_f32( diff, float32v( 0.5f ) );
            diff = FS_Select_f32( diffSign, float32v( 0.5f ) - diff, diff - float32v( 0.5f ) );

            rounded += diff;
        }

        return rounded * float32v( mMultiplierRecip );
    }
};

template<typename FS>
class FS_T<FastNoise::DomainAxisScale, FS> : public virtual FastNoise::DomainAxisScale, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        size_t idx = 0;
        ((pos *= float32v( mScale[idx++] )), ...);

        return this->GetSourceValue( mSource, seed, pos... );
    }
};

template<typename FS>
class FS_T<FastNoise::AddDimension, FS> : public virtual FastNoise::AddDimension, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        if constexpr( sizeof...(P) == (size_t)FastNoise::Dim::Count )
        {
            return this->GetSourceValue( mSource, seed, pos... );
        }
        else
        {
            return this->GetSourceValue( mSource, seed, pos..., this->GetSourceValue( mNewDimensionPosition, seed, pos... ) );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::RemoveDimension, FS> : public virtual FastNoise::RemoveDimension, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        return this->GetSourceValue( mSource, seed, x, y );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        switch( mRemoveDimension )
        {
        case FastNoise::Dim::X:
            return this->GetSourceValue( mSource, seed, y, z );
        case FastNoise::Dim::Y:
            return this->GetSourceValue( mSource, seed, x, z );
        case FastNoise::Dim::Z:
            return this->GetSourceValue( mSource, seed, x, y );
        default:
            return this->GetSourceValue( mSource, seed, x, y, z );
        }
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        switch( mRemoveDimension )
        {
        case FastNoise::Dim::X:
            return this->GetSourceValue( mSource, seed, y, z, w );
        case FastNoise::Dim::Y:
            return this->GetSourceValue( mSource, seed, x, z, w );
        case FastNoise::Dim::Z:
            return this->GetSourceValue( mSource, seed, x, y, w );
        case FastNoise::Dim::W:
            return this->GetSourceValue( mSource, seed, x, y, z );
        default:
            return this->GetSourceValue( mSource, seed, x, y, z, w );
        }
    }
};

template<typename FS>
class FS_T<FastNoise::GeneratorCache, FS> : public virtual FastNoise::GeneratorCache, public FS_T<FastNoise::Generator, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;
    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        thread_local static const void* CachedGenerator = nullptr;
        thread_local static float CachedValue[FS_Size_32()];
        thread_local static float CachedPos[FS_Size_32()][sizeof...( P )];
        // TLS is not always aligned (compiler bug), need to avoid using SIMD types

        float32v arrayPos[] = { pos... };

        bool isSame = (CachedGenerator == mSource.simdGeneratorPtr);

        for( size_t i = 0; i < sizeof...( P ); i++ )
        {
            isSame &= !FS_AnyMask_bool( arrayPos[i] != FS_Load_f32( &CachedPos[i] ) );
        }

        if( !isSame )
        {
            CachedGenerator = mSource.simdGeneratorPtr;

            float32v value = this->GetSourceValue( mSource, seed, pos... );
            FS_Store_f32( &CachedValue, value );

            for( size_t i = 0; i < sizeof...(P); i++ )
            {
                FS_Store_f32( &CachedPos[i], arrayPos[i] );
            }

            return value;
        }

        return FS_Load_f32( &CachedValue );
    }
};
