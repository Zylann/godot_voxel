#pragma once
#include "Generator.h"

#include <algorithm>

namespace FastNoise
{
    class Cellular : public virtual Generator
    {
    public:
        void SetJitterModifier( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mJitterModifier, gen ); }
        void SetJitterModifier( float value ) { mJitterModifier = value; }
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

    protected:
        HybridSource mJitterModifier = 1.0f;
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Cellular> : MetadataT<Generator>
    {
        MetadataT()
        {
            groups.push_back( "Coherent Noise" );
            this->AddHybridSource( "Jitter Modifier", 1.0f, &Cellular::SetJitterModifier, &Cellular::SetJitterModifier );
            this->AddVariableEnum( "Distance Function", DistanceFunction::EuclideanSquared, &Cellular::SetDistanceFunction, kDistanceFunction_Strings );
        }
    };
#endif

    class CellularValue : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        static const int kMaxDistanceCount = 4;

        void SetValueIndex( int value ) { mValueIndex = std::min( std::max( value, 0 ), kMaxDistanceCount - 1 ); }

    protected:
        int mValueIndex = 0;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<CellularValue> : MetadataT<Cellular>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddVariable( "Value Index", 0, &CellularValue::SetValueIndex, 0, CellularValue::kMaxDistanceCount - 1 );
        }
    };
#endif

    class CellularDistance : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        enum class ReturnType
        {
            Index0,
            Index0Add1,
            Index0Sub1,
            Index0Mul1,
            Index0Div1
        };

        static const int kMaxDistanceCount = 4;

        void SetDistanceIndex0( int value ) { mDistanceIndex0 = std::min( std::max( value, 0 ), kMaxDistanceCount - 1 ); }
        void SetDistanceIndex1( int value ) { mDistanceIndex1 = std::min( std::max( value, 0 ), kMaxDistanceCount - 1 ); }
        void SetReturnType( ReturnType value ) { mReturnType = value; }

    protected:
        ReturnType mReturnType = ReturnType::Index0;
        int mDistanceIndex0 = 0;
        int mDistanceIndex1 = 1;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<CellularDistance> : MetadataT<Cellular>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddVariable( "Distance Index 0", 0, &CellularDistance::SetDistanceIndex0, 0, CellularDistance::kMaxDistanceCount - 1 );
            this->AddVariable( "Distance Index 1", 1, &CellularDistance::SetDistanceIndex1, 0, CellularDistance::kMaxDistanceCount - 1 );
            this->AddVariableEnum( "Return Type", CellularDistance::ReturnType::Index0, &CellularDistance::SetReturnType, "Index0", "Index0Add1", "Index0Sub1", "Index0Mul1", "Index0Div1" );
        }
    };
#endif

    class CellularLookup : public virtual Cellular
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetLookup( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLookup, gen ); }
        void SetLookupFrequency( float freq ) { mLookupFreq = freq; }

    protected:
        GeneratorSource mLookup;
        float mLookupFreq = 0.1f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<CellularLookup> : MetadataT<Cellular>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddGeneratorSource( "Lookup", &CellularLookup::SetLookup );
            this->AddVariable( "Lookup Frequency", 0.1f, &CellularLookup::SetLookupFrequency );
        }
    };
#endif
}
