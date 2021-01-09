#pragma once
#include "Fractal.h"
#include "DomainWarp.h"

namespace FastNoise
{
    class DomainWarpFractalProgressive : public virtual Fractal<DomainWarp>
    {
        FASTNOISE_METADATA( Fractal<DomainWarp> )
            Metadata( const char* className ) : Fractal<DomainWarp>::Metadata( className, "Domain Warp Source" )
            {
                groups.push_back( "Domain Warp" );
            }
        };    
    };

    class DomainWarpFractalIndependant : public virtual Fractal<DomainWarp>
    {
        FASTNOISE_METADATA( Fractal<DomainWarp> )
            Metadata( const char* className ) : Fractal<DomainWarp>::Metadata( className, "Domain Warp Source" )
            {
                groups.push_back( "Domain Warp" );
            }
        };    
    };
}
