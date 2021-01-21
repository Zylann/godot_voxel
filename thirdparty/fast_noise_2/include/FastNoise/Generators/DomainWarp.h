#pragma once
#include "Generator.h"

namespace FastNoise
{
    class DomainWarp : public virtual Generator
    {
    public:
        void SetSource( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mSource, gen ); }
        void SetWarpAmplitude( SmartNodeArg<> gen ) { this->SetSourceMemberVariable( mWarpAmplitude, gen ); }
        void SetWarpAmplitude( float value ) { mWarpAmplitude = value; } 
        void SetWarpFrequency( float value ) { mWarpFrequency = value; }

    protected:
        GeneratorSource mSource;
        HybridSource mWarpAmplitude = 1.0f;
        float mWarpFrequency = 0.5f;

        FASTNOISE_METADATA_ABSTRACT( Generator )
        
            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Domain Warp" );                
                this->AddGeneratorSource( "Source", &DomainWarp::SetSource );
                this->AddHybridSource( "Warp Amplitude", 1.0f, &DomainWarp::SetWarpAmplitude, &DomainWarp::SetWarpAmplitude );
                this->AddVariable( "Warp Frequency", 0.5f, &DomainWarp::SetWarpFrequency );
            }
        };
    };

    class DomainWarpGradient : public virtual DomainWarp
    {
        FASTNOISE_METADATA( DomainWarp )
            using DomainWarp::Metadata::Metadata;
        };        
    };
}
