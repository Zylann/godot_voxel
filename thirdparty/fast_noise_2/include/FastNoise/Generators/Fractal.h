#pragma once
#include "Generator.h"

namespace FastNoise
{
    template<typename T = Generator>
    class Fractal : public virtual Generator
    {
    public:
        void SetSource( SmartNodeArg<T> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetGain( float value ) { mGain = value; CalculateFractalBounding(); } 
        void SetGain( SmartNodeArg<> gen ) { mGain = 1.0f; this->SetSourceMemberVariable( mGain, gen ); CalculateFractalBounding(); }
        void SetOctaveCount( int32_t value ) { mOctaves = value; CalculateFractalBounding(); } 
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:
        GeneratorSourceT<T> mSource;
        HybridSource mGain = 0.5f;

        int32_t mOctaves = 3;
        float mLacunarity = 2.0f;
        float mFractalBounding = 1.0f / 1.75f;

        virtual void CalculateFractalBounding()
        {
            float gain = std::abs( mGain.constant );
            float amp = gain;
            float ampFractal = 1.0f;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                ampFractal += amp;
                amp *= gain;
            }
            mFractalBounding = 1.0f / ampFractal;
        }

        FASTNOISE_METADATA_ABSTRACT( Generator )

            Metadata( const char* className, const char* sourceName = "Source" ) : Generator::Metadata( className )
            {
                groups.push_back( "Fractal" );

                this->AddGeneratorSource( sourceName, &Fractal::SetSource );
                this->AddHybridSource( "Gain", 0.5f, &Fractal::SetGain, &Fractal::SetGain );
                this->AddVariable( "Octaves", 3, &Fractal::SetOctaveCount, 2, 16 );
                this->AddVariable( "Lacunarity", 2.0f, &Fractal::SetLacunarity );
            }
        };        
    };

    class FractalFBm : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )        
            using Fractal::Metadata::Metadata;
        };    
    };

    class FractalBillow : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )
            using Fractal::Metadata::Metadata;
        };    
    };

    class FractalRidged : public virtual Fractal<>
    {
        FASTNOISE_METADATA( Fractal )
            using Fractal::Metadata::Metadata;
        };              
    };

    class FractalRidgedMulti : public virtual Fractal<>
    {
    public:
        void SetWeightAmplitude( float value ) { mWeightAmp = value; CalculateFractalBounding(); }

    protected:
        float mWeightAmp = 2.0f;
        float mWeightBounding = 2.0f / 1.75f;

        void CalculateFractalBounding() override
        {
            Fractal::CalculateFractalBounding();

            float weight = 1.0f;
            float totalWeight = weight;
            for( int32_t i = 1; i < mOctaves; i++ )
            {
                weight *= mWeightAmp;
                totalWeight += 1.0f / weight;
            }
            mWeightBounding = 2.0f / totalWeight;
        }

        FASTNOISE_METADATA( Fractal )

            Metadata( const char* className ) : Fractal::Metadata( className )
            {
                this->AddVariable( "Weight Amplitude", 2.0f, &FractalRidgedMulti::SetWeightAmplitude );
            }
        };      
    };
}
