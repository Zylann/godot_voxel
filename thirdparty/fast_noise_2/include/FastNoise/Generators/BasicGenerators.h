#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Constant : public virtual Generator
    {
    public:
        void SetValue( float value ) { mValue = value; }

    protected:
        float mValue = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
                this->AddVariable( "Value", 1.0f, &Constant::SetValue );
            }
        };
    };

    class White : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
            }
        };
    };  

    class Checkerboard : public virtual Generator
    {
    public:
        void SetSize( float value ) { mSize = value; }

    protected:
        float mSize = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
                this->AddVariable( "Size", 1.0f, &Checkerboard::SetSize );
            }
        };
    };

    class SineWave : public virtual Generator
    {
    public:
        void SetScale( float value ) { mScale = value; }

    protected:
        float mScale = 1.0f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
                this->AddVariable( "Scale", 1.0f, &SineWave::SetScale );
            }
        };
    };

    class PositionOutput : public virtual Generator
    {
    public:
        template<Dim D>
        void Set( float multiplier, float offset = 0.0f ) { mMultiplier[(int)D] = multiplier; mOffset[(int)D] = offset; }

    protected:
        PerDimensionVariable<float> mMultiplier;
        PerDimensionVariable<float> mOffset;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
                this->AddPerDimensionVariable( "Multiplier", 0.0f, []( PositionOutput* p ) { return std::ref( p->mMultiplier ); } );
                this->AddPerDimensionVariable( "Offset",     0.0f, []( PositionOutput* p ) { return std::ref( p->mOffset ); } );
            }
        };
    };

    class DistanceToOrigin : public virtual Generator
    {
    public:
        void SetDistanceFunction( DistanceFunction value ) { mDistanceFunction = value; }

    protected:
        DistanceFunction mDistanceFunction = DistanceFunction::EuclideanSquared;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Basic Generators" );
                this->AddVariableEnum( "Distance Function", DistanceFunction::Euclidean, &DistanceToOrigin::SetDistanceFunction, "Euclidean", "Euclidean Squared", "Manhattan", "Hybrid" );
            }
        };
    };
}
