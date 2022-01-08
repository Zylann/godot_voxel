#pragma once
#include "Generator.h"

#include <climits>

namespace FastNoise
{
    class OperatorSourceLHS : public virtual Generator
    {
    public:
        void SetLHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLHS, gen ); }
        void SetRHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mRHS, gen ); }
        void SetRHS( float value ) { mRHS = value; }

    protected:
        GeneratorSource mLHS;
        HybridSource mRHS = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OperatorSourceLHS> : MetadataT<Generator>
    {
        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddGeneratorSource( "LHS", &OperatorSourceLHS::SetLHS );
            this->AddHybridSource( "RHS", 0.0f, &OperatorSourceLHS::SetRHS, &OperatorSourceLHS::SetRHS );
        }
    };
#endif

    class OperatorHybridLHS : public virtual Generator
    {
    public:
        void SetLHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLHS, gen ); }
        void SetLHS( float value ) { mLHS = value; }
        void SetRHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mRHS, gen ); }
        void SetRHS( float value ) { mRHS = value; }

    protected:
        HybridSource mLHS = 0.0f;
        HybridSource mRHS = 0.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OperatorHybridLHS> : MetadataT<Generator>
    {
        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddHybridSource( "LHS", 0.0f, &OperatorHybridLHS::SetLHS, &OperatorHybridLHS::SetLHS );
            this->AddHybridSource( "RHS", 0.0f, &OperatorHybridLHS::SetRHS, &OperatorHybridLHS::SetRHS );
        }
    };
#endif

    class Add : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Add> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class Subtract : public virtual OperatorHybridLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Subtract> : MetadataT<OperatorHybridLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class Multiply : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Multiply> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class Divide : public virtual OperatorHybridLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Divide> : MetadataT<OperatorHybridLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class Min : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Min> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class Max : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Max> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;
    };
#endif

    class PowFloat : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetValue( float value ) { mValue = value; }
        void SetPow( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPow, gen ); }
        void SetPow( float value ) { mPow = value; }

    protected:
        HybridSource mValue = 2.0f;
        HybridSource mPow = 2.0f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PowFloat> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddHybridSource( "Value", 2.0f, &PowFloat::SetValue, &PowFloat::SetValue );
            this->AddHybridSource( "Pow", 2.0f, &PowFloat::SetPow, &PowFloat::SetPow );
        }
    };
#endif

    class PowInt : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetPow( int value ) { mPow = value; }

    protected:
        GeneratorSource mValue;
        int mPow = 2;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<PowInt> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddGeneratorSource( "Value", &PowInt::SetValue );
            this->AddVariable( "Pow", 2, &PowInt::SetPow, 2, INT_MAX );
        }
    };
#endif

    class MinSmooth : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<MinSmooth> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddHybridSource( "Smoothness", 0.1f, &MinSmooth::SetSmoothness, &MinSmooth::SetSmoothness );
        }
    };
#endif

    class MaxSmooth : public virtual OperatorSourceLHS
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;

        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;  
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<MaxSmooth> : MetadataT<OperatorSourceLHS>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            this->AddHybridSource( "Smoothness", 0.1f, &MaxSmooth::SetSmoothness, &MaxSmooth::SetSmoothness );
        }
    };
#endif

    class Fade : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
        void SetA( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mA, gen ); }
        void SetB( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mB, gen ); }

        void SetFade( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFade, gen ); }
        void SetFade( float value ) { mFade = value; }

    protected:
        GeneratorSource mA;
        GeneratorSource mB;
        HybridSource mFade = 0.5f;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Fade> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Blends" );
            this->AddGeneratorSource( "A", &Fade::SetA );
            this->AddGeneratorSource( "B", &Fade::SetB );
            this->AddHybridSource( "Fade", 0.5f, &Fade::SetFade, &Fade::SetFade );
        }
    };
#endif
}
