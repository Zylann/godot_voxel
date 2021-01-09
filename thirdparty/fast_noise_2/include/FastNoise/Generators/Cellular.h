#pragma once

#include "Generator.h"

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

        const float kJitter2D = 0.437015f;
        const float kJitter3D = 0.396143f;
        const float kJitter4D = 0.366025f;

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Coherent Noise" );
                this->AddHybridSource( "Jitter Modifier", 1.0f, &Cellular::SetJitterModifier, &Cellular::SetJitterModifier );
                this->AddVariableEnum( "Distance Function", DistanceFunction::EuclideanSquared, &Cellular::SetDistanceFunction, "Euclidean", "Euclidean Squared", "Manhattan", "Hybrid" );
            }
        };
    };

    class CellularValue : public virtual Cellular
    {
    public:
        void SetValueIndex( int value ) { mValueIndex = value; }

    protected:
        static const int kMaxDistanceCount = 4;

        int mValueIndex = 0;

        FASTNOISE_METADATA( Cellular )
            Metadata( const char* className ) : Cellular::Metadata( className )
            {
                this->AddVariable( "Value Index", 0, &CellularValue::SetValueIndex, 0, kMaxDistanceCount - 1 );
            }
        };
    };

    class CellularDistance : public virtual Cellular
    {
    public:
        enum class ReturnType
        {
            Index0,
            Index0Add1,
            Index0Sub1,
            Index0Mul1,
            Index0Div1
        };

        void SetDistanceIndex0( int value ) { mDistanceIndex0 = value; }
        void SetDistanceIndex1( int value ) { mDistanceIndex1 = value; }
        void SetReturnType( ReturnType value ) { mReturnType = value; }

    protected:
        static const int kMaxDistanceCount = 4;

        ReturnType mReturnType = ReturnType::Index0;
        int mDistanceIndex0 = 0;
        int mDistanceIndex1 = 1;

        FASTNOISE_METADATA( Cellular )

            Metadata( const char* className ) : Cellular::Metadata( className )
            {
                this->AddVariable( "Distance Index 0", 0, &CellularDistance::SetDistanceIndex0, 0, kMaxDistanceCount - 1 );
                this->AddVariable( "Distance Index 1", 1, &CellularDistance::SetDistanceIndex1, 0, kMaxDistanceCount - 1 );
                this->AddVariableEnum( "Return Type", ReturnType::Index0, &CellularDistance::SetReturnType, "Index0", "Index0Add1", "Index0Sub1", "Index0Mul1", "Index0Div1" );
            }
        };
    };

    class CellularLookup : public virtual Cellular
    {
    public:
        void SetLookup( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLookup, gen ); }
        void SetLookupFrequency( float freq ) { mLookupFreq = freq; }

    protected:
        GeneratorSource mLookup;
        float mLookupFreq = 0.1f;

        FASTNOISE_METADATA( Cellular )
        
            Metadata( const char* className ) : Cellular::Metadata( className )
            {
                this->AddGeneratorSource( "Lookup", &CellularLookup::SetLookup );
                this->AddVariable( "Lookup Frequency", 0.1f, &CellularLookup::SetLookupFrequency );
            }
        };
    };
}
