#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<Simplex> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from an N dimensional simplex grid\n"
                "Developed by Ken Perlin in 2001";
        }
    };
#endif

    class OpenSimplex2 : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OpenSimplex2> : MetadataT<Generator>
    {
        SmartNode<> CreateNode( FastSIMD::eLevel ) const override;

        MetadataT()
        {
            groups.push_back( "Coherent Noise" );

            description = 
                "Smooth gradient noise from an N dimensional simplex grid, alternate implementation\n"
                "Developed by K.jpg in 2019";
        }
    };
#endif

    class OpenSimplex2S : public virtual Generator
    {
    public:
        FASTSIMD_LEVEL_SUPPORT(FastNoise::SUPPORTED_SIMD_LEVELS);
        const Metadata& GetMetadata() const override;
    };

#ifdef FASTNOISE_METADATA
    template<>
    struct MetadataT<OpenSimplex2S> : MetadataT<Generator>
    {
        SmartNode<> CreateNode(FastSIMD::eLevel) const override;

        MetadataT()
        {
            groups.push_back("Coherent Noise");

            description =
                "Smoother gradient noise from an N dimensional simplex grid\n"
                "Developed by K.jpg in 2017";
        }
    };
#endif
}
