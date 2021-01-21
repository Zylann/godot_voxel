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
        HybridSource mRHS;

        FASTNOISE_METADATA_ABSTRACT( Generator )
            
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Blends" );
                this->AddGeneratorSource( "LHS", &OperatorSourceLHS::SetLHS );
                this->AddHybridSource( "RHS", 0.0f, &OperatorSourceLHS::SetRHS, &OperatorSourceLHS::SetRHS );
            }
        };
    };

    class OperatorHybridLHS : public virtual Generator
    {
    public:
        void SetLHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mLHS, gen ); }
        void SetLHS( float value ) { mLHS = value; }
        void SetRHS( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mRHS, gen ); }
        void SetRHS( float value ) { mRHS = value; }

    protected:
        HybridSource mLHS;
        HybridSource mRHS;

        FASTNOISE_METADATA_ABSTRACT( Generator )
            
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Blends" );
                this->AddHybridSource( "LHS", 0.0f, &OperatorHybridLHS::SetLHS, &OperatorHybridLHS::SetLHS );
                this->AddHybridSource( "RHS", 0.0f, &OperatorHybridLHS::SetRHS, &OperatorHybridLHS::SetRHS );
            }
        };
    };

    class Add : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Subtract : public virtual OperatorHybridLHS
    {
        FASTNOISE_METADATA( OperatorHybridLHS )
            using OperatorHybridLHS::Metadata::Metadata;
        };    
    };

    class Multiply : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Divide : public virtual OperatorHybridLHS
    {
        FASTNOISE_METADATA( OperatorHybridLHS )
            using OperatorHybridLHS::Metadata::Metadata;
        };    
    };

    class Min : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };

    class Max : public virtual OperatorSourceLHS
    {
        FASTNOISE_METADATA( OperatorSourceLHS )
            using OperatorSourceLHS::Metadata::Metadata;
        };    
    };    

    class PowFloat : public virtual Generator
    {
    public:
        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetValue( float value ) { mValue = value; }
        void SetPow( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mPow, gen ); }
        void SetPow( float value ) { mPow = value; }

    protected:
        HybridSource mValue;
        HybridSource mPow;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Blends" );
                this->AddHybridSource( "Value", 2.0f, &PowFloat::SetValue, &PowFloat::SetValue );
                this->AddHybridSource( "Pow", 2.0f, &PowFloat::SetPow, &PowFloat::SetPow );
            }
        };
    };

    class PowInt : public virtual OperatorHybridLHS
    {
    public:
        void SetValue( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mValue, gen ); }
        void SetPow( int32_t value ) { mPow = value; }

    protected:
        GeneratorSource mValue;
        int32_t mPow;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Blends" );
                this->AddGeneratorSource( "Value", &PowInt::SetValue );
                this->AddVariable( "Pow", 2, &PowInt::SetPow, 2, INT_MAX );
            }
        };
    };

    class MinSmooth : public virtual OperatorSourceLHS
    {
    public:
        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;

        FASTNOISE_METADATA( OperatorSourceLHS )
            Metadata( const char* className ) : OperatorSourceLHS::Metadata( className )
            {
                this->AddHybridSource( "Smoothness", 0.1f, &MinSmooth::SetSmoothness, &MinSmooth::SetSmoothness );
            }
        };    
    };

    class MaxSmooth : public virtual OperatorSourceLHS
    {
    public:
        void SetSmoothness( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSmoothness, gen ); }
        void SetSmoothness( float value ) { mSmoothness = value; }

    protected:
        HybridSource mSmoothness = 0.1f;

        FASTNOISE_METADATA( OperatorSourceLHS )
            Metadata( const char* className ) : OperatorSourceLHS::Metadata( className )
            {
                this->AddHybridSource( "Smoothness", 0.1f, &MaxSmooth::SetSmoothness, &MaxSmooth::SetSmoothness );
            }
        };    
    };

    class Fade : public virtual Generator
    {
    public:
        void SetA( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mA, gen ); }
        void SetB( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mB, gen ); }

        void SetFade( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mFade, gen ); }
        void SetFade( float value ) { mFade = value; }

    protected:
        GeneratorSource mA;
        GeneratorSource mB;
        HybridSource mFade = 0.5f;

        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Blends" );
                this->AddGeneratorSource( "A", &Fade::SetA );
                this->AddGeneratorSource( "B", &Fade::SetB );
                this->AddHybridSource( "Fade", 0.5f, &Fade::SetFade, &Fade::SetFade );
            }
        };    
    };
}
