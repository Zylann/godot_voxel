#pragma once
#include "Generator.h"

namespace FastNoise
{
    class Value : public virtual Generator
    {
        FASTNOISE_METADATA( Generator )

            Metadata( const char* className ) : Generator::Metadata( className )
            {
                groups.push_back( "Coherent Noise" );
            }
        };            
    };
}
