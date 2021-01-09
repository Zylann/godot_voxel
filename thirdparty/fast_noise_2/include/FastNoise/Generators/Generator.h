#pragma once
#include <cassert>
#include <cmath>

#include "FastNoise/FastNoiseMetadata.h"

namespace FastNoise
{
    enum class Dim
    {
        X, Y, Z, W,
        Count
    };

    enum class DistanceFunction
    {
        Euclidean,
        EuclideanSquared,
        Manhattan,
        Hybrid,
    };

    struct OutputMinMax
    {
        float min = INFINITY;
        float max = -INFINITY;

        OutputMinMax& operator <<( float v )
        {
            min = fminf( min, v );
            max = fmaxf( max, v );
            return *this;
        }

        OutputMinMax& operator <<( const OutputMinMax& v )
        {
            min = fminf( min, v.min );
            max = fmaxf( max, v.max );
            return *this;
        }
    };

    template<typename T>
    struct BaseSource
    {
        using Type = T;

        SmartNode<T> base;
        void* simdGeneratorPtr = nullptr;

    protected:
        BaseSource() = default;
    };

    template<typename T>
    struct GeneratorSourceT : BaseSource<T>
    { };

    template<typename T>
    struct HybridSourceT : BaseSource<T>
    {
        float constant;

        HybridSourceT( float f = 0.0f )
        {
            constant = f;
        }
    };

    class Generator
    {
    public:
        using Metadata = FastNoise::Metadata;
        friend Metadata;

        virtual ~Generator() = default;

        virtual FastSIMD::eLevel GetSIMDLevel() const = 0;
        virtual const Metadata* GetMetadata() const = 0;

        virtual OutputMinMax GenUniformGrid2D( float* noiseOut,
            int32_t xStart, int32_t yStart,
            int32_t xSize, int32_t ySize,
            float frequency, int32_t seed ) const = 0;

        virtual OutputMinMax GenUniformGrid3D( float* noiseOut,
            int32_t xStart, int32_t yStart, int32_t zStart,
            int32_t xSize,  int32_t ySize,  int32_t zSize,
            float frequency, int32_t seed ) const = 0;

        virtual OutputMinMax GenPositionArray2D( float* noiseOut, int32_t count,
            const float* xPosArray, const float* yPosArray,
            float xOffset, float yOffset, int32_t seed ) const = 0;

        virtual OutputMinMax GenPositionArray3D( float* noiseOut, int32_t count,
            const float* xPosArray, const float* yPosArray, const float* zPosArray, 
            float xOffset, float yOffset, float zOffset, int32_t seed ) const = 0;

        virtual OutputMinMax GenTileable2D( float* noiseOut,
            int32_t xSize,  int32_t ySize, 
            float frequency, int32_t seed ) const = 0;  


    protected:
        template<typename T>
        void SetSourceMemberVariable( BaseSource<T>& memberVariable, SmartNodeArg<T> gen )
        {
            static_assert( std::is_base_of_v<Generator, T> );
            assert( gen.get() );
            assert( GetSIMDLevel() == gen->GetSIMDLevel() ); // Ensure that all SIMD levels match

            memberVariable.base = gen;
            SetSourceSIMDPtr( dynamic_cast<Generator*>( gen.get() ), &memberVariable.simdGeneratorPtr );
        }

    private:
        virtual void SetSourceSIMDPtr( Generator* base, void** simdPtr ) = 0;
    };

    using GeneratorSource = GeneratorSourceT<Generator>;
    using HybridSource = HybridSourceT<Generator>;

    template<typename T>
    struct PerDimensionVariable
    {
        using Type = T;

        T varArray[(int)Dim::Count];

        template<typename U = T>
        PerDimensionVariable( U value = 0 )
        {
            for( T& element : varArray )
            {
                element = value;
            }
        }

        T& operator[]( size_t i )
        {
            return varArray[i];
        }

        const T& operator[]( size_t i ) const
        {
            return varArray[i];
        }
    };
}
