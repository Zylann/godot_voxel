#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Simplex : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Coherent Noise" );
            }
        };            
    };

    class OpenSimplex2 : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Coherent Noise" );
            }
        };            
    };
}
