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
        void SetWeightedStrength( float value ) { mWeightedStrength = value; } 
        void SetWeightedStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWeightedStrength, gen ); }
        void SetOctaveCount( int value ) { mOctaves = value; CalculateFractalBounding(); } 
        void SetLacunarity( float value ) { mLacunarity = value; } 

    protected:
        GeneratorSourceT<T> mSource;
        HybridSource mGain = 0.5f;
        HybridSource mWeightedStrength = 0.0f;

        int   mOctaves = 3;
        float mLacunarity = 2.0f;
        float mFractalBounding = 1.0f / 1.75f;

        void CalculateFractalBounding()
        {
            float gain = std::abs( mGain.constant );
            float amp = gain;
            float ampFractal = 1.0f;
            for( int i = 1; i < mOctaves; i++ )
            {
                ampFractal += amp;
                amp *= gain;
            }
            mFractalBounding = 1.0f / ampFractal;
        }     
    };

#ifdef FASTNOISE_METADATA
    template<typename T>
    struct MetadataT<Fractal<T>> : MetadataT<Generator>
    {
        MetadataT( const char* sourceName = "Source" )
        {
            groups.push_back( "Fractal" );

            this->AddGeneratorSource( sourceName, &Fractal<T>::SetSource );
            this->AddHybridSource( "Gain", 0.5f, &Fractal<T>::SetGain, &Fractal<T>::SetGain );
            this->AddHybridSource( "Weighted Strength", 0.0f, &Fractal<T>::SetWeightedStrength, &Fractal<T>::SetWeightedStrength );
            this->AddVariable( "Octaves", 3, &Fractal<T>::SetOctaveCount, 2, 16 );
            this->AddVariable( "Lacunarity", 2.0f, &Fractal<T>::SetLacunarity );
        }
    };
#endif

    class FractalFBm : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<FractalFBm> : MetadataT<Fractal<>>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class FractalRidged : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<FractalRidged> : MetadataT<Fractal<>>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class FractalPingPong : public virtual Fractal<>
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetPingPongStrength( float value ) { mPingPongStrength = value; }
        void SetPingPongStrength( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPingPongStrength, gen ); }

    protected:
        HybridSource mPingPongStrength = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<FractalPingPong> : MetadataT<Fractal<>>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddHybridSource( "Ping Pong Strength", 2.0f, &FractalPingPong::SetPingPongStrength, &FractalPingPong::SetPingPongStrength );
        }
    };
#endif
}
