#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Constant : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Constant> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Value", 1.0f, &Constant::SetValue );
        }
    };
#endif

    class White : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<White> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
        }
    };
#endif

    class Checkerboard : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSize( float value ) { mSize = value; }

    protected:
        float mSize = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Checkerboard> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Size", 1.0f, &Checkerboard::SetSize );
        }
    };
#endif

    class SineWave : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetScale( float value ) { mScale = value; }

    protected:
        float mScale = 1.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<SineWave> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariable( "Scale", 1.0f, &SineWave::SetScale );
        }
    };
#endif

    class PositionOutput : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        template<Dim D>
        void Set( float multiplier, float offset = 0.0f ) { mMultiplier[(int)D] = multiplier; mOffset[(int)D] = offset; }

    protected:
        PerDimensionVariable<float> mMultiplier = 0.0f;
        PerDimensionVariable<float> mOffset = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PositionOutput> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddPerDimensionVariable( "Multiplier", 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
            this->AddPerDimensionVariable( "Offset", 0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );
        }
    };
#endif

    class DistanceToPoint : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

        template<Dim D>
        void SetScale( float value ) { mPoint[(int)D] = value; }

    protected:
        GeneratorSource mSource;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;
        PerDimensionVariable<float> mPoint = 0.0f;

        template<typename T>
        friend struct MetadataT;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<DistanceToPoint> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Basic Generators" );
            this->AddVariableEnum( "Distance Function", DistanceFunction::Euclidean, &DistanceToPoint::SetDistanceFunction, kDistanceFunction_Strings );
            this->AddPerDimensionVariable( "Point", 0.0f, []( DistanceToPoint* p ) { return std::ref( p->mPoint ); } );
        }
    };
#endif
}
