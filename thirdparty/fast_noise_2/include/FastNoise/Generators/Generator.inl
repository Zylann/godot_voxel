#include <cassert>
#include <cstring>
#include "FastSIMD/InlInclude.h"

#include "Generator.h"

#ifdef FS_SIMD_CLASS
#pragma warning( disable:4250 )
#endif

template<typename FS>
class FS_T<FastNoise::Generator, FS> : public virtual FastNoise::Generator
{
    FASTSIMD_DECLARE_FS_TYPES;

public:
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const = 0;
    virtual float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const { return Gen( seed, x, y, z ); };

#define FASTNOISE_IMPL_GEN_T\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const override { return GenT( seed, x, y ); }\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const override { return GenT( seed, x, y, z ); }\
    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const override { return GenT( seed, x, y, z, w ); }

    FastSIMD::eLevel GetSIMDLevel() const final
    {
        return FS::SIMD_Level;
    }

    using VoidPtrStorageType = FS_T<Generator, FS>*;

    void SetSourceSIMDPtr( Generator* base, void** simdPtr ) final
    {
        auto simd = dynamic_cast<VoidPtrStorageType>( base );
        assert( simd );
        *simdPtr = reinterpret_cast<void*>( simd );
    }

    template<typename T, typename... POS>
    FS_INLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::HybridSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        if( memberVariable.simdGeneratorPtr )
        {
            auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

            return simdGen->Gen( seed, pos... );
        }
        return float32v( memberVariable.constant );
    }

    template<typename T, typename... POS>
    FS_INLINE float32v FS_VECTORCALL GetSourceValue( const FastNoise::GeneratorSourceT<T>& memberVariable, int32v seed, POS... pos ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        return simdGen->Gen( seed, pos... );
    }

    template<typename T>
    FS_INLINE const FS_T<T, FS>* GetSourceSIMD( const FastNoise::GeneratorSourceT<T>& memberVariable ) const
    {
        assert( memberVariable.simdGeneratorPtr );
        auto simdGen = reinterpret_cast<VoidPtrStorageType>( memberVariable.simdGeneratorPtr );

        auto simdT = static_cast<FS_T<T, FS>*>( simdGen );
        return simdT;
    }

    FastNoise::OutputMinMax GenUniformGrid2D( float* noiseOut, int32_t xStart, int32_t yStart, int32_t xSize, int32_t ySize, float frequency, int32_t seed ) const final
    {
        assert( xSize >= (int32_t)FS_Size_32() );

        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        size_t totalValues = xSize * ySize;
        size_t index = 0;

        xIdx += int32v::FS_Incremented();

        while( index < totalValues - FS_Size_32() )
        {
            float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
            float32v yPos = FS_Converti32_f32( yIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif

            index += FS_Size_32();
            xIdx += int32v( FS_Size_32() );

            mask32v xReset = xIdx > xMax;
            yIdx = FS_MaskedIncrement_i32( yIdx, xReset );
            xIdx = FS_MaskedSub_i32( xIdx, xSizeV, xReset );
        }

        float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
        float32v yPos = FS_Converti32_f32( yIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid3D( float* noiseOut, int32_t xStart, int32_t yStart, int32_t zStart, int32_t xSize, int32_t ySize, int32_t zSize, float frequency, int32_t seed ) const final
    {
        assert( xSize >= (int32_t)FS_Size_32() );

        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );
        int32v zIdx( zStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );
        int32v ySizeV( ySize );
        int32v yMax = ySizeV + yIdx + int32v( -1 );

        size_t totalValues = xSize * ySize * zSize;
        size_t index = 0;

        xIdx += int32v::FS_Incremented();

        while( index < totalValues - FS_Size_32() )
        {
            float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
            float32v yPos = FS_Converti32_f32( yIdx ) * freqV;
            float32v zPos = FS_Converti32_f32( zIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif

            index += FS_Size_32();
            xIdx += int32v( FS_Size_32() );

            mask32v xReset = xIdx > xMax;
            yIdx = FS_MaskedIncrement_i32( yIdx, xReset );
            xIdx = FS_MaskedSub_i32( xIdx, xSizeV, xReset );

            mask32v yReset = yIdx > yMax;
            zIdx = FS_MaskedIncrement_i32( zIdx, yReset );
            yIdx = FS_MaskedSub_i32( yIdx, ySizeV, yReset );
        }

        float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
        float32v yPos = FS_Converti32_f32( yIdx ) * freqV;
        float32v zPos = FS_Converti32_f32( zIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray2D( float* noiseOut, int32_t count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int32_t seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        size_t index = 0;
        while( index < count - FS_Size_32() )
        {
            float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif
            index += FS_Size_32();
        }

        float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray3D( float* noiseOut, int32_t count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int32_t seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32_t index = 0;
        while( index < int64_t(count) - FS_Size_32() )
        {
            float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );
            float32v zPos = float32v( zOffset ) + FS_Load_f32( &zPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif
            index += FS_Size_32();
        }

        float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
        float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );
        float32v zPos = float32v( zOffset ) + FS_Load_f32( &zPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenTileable2D( float* noiseOut, int32_t xSize, int32_t ySize, float frequency, int32_t seed ) const final
    {
        assert( xSize >= (int32_t)FS_Size_32() );

        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( 0 );
        int32v yIdx( 0 );

        int32v xSizeV( xSize );
        int32v ySizeV( ySize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        size_t totalValues = xSize * ySize;
        size_t index = 0;

        float pi2Recip( 0.15915493667f );
        float xSizePi = (float)xSize * pi2Recip;
        float ySizePi = (float)ySize * pi2Recip;
        float32v xFreq = float32v( frequency * xSizePi );
        float32v yFreq = float32v( frequency * ySizePi );
        float32v xMul = float32v( 1 / xSizePi );
        float32v yMul = float32v( 1 / ySizePi );

        xIdx += int32v::FS_Incremented();

        while( index < totalValues - FS_Size_32() )
        {
            float32v xF = FS_Converti32_f32( xIdx ) * xMul;
            float32v yF = FS_Converti32_f32( yIdx ) * yMul;

            float32v xPos = FS_Cos_f32( xF ) * xFreq;
            float32v yPos = FS_Cos_f32( yF ) * yFreq;
            float32v zPos = FS_Sin_f32( xF ) * xFreq;
            float32v wPos = FS_Sin_f32( yF ) * yFreq;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif

            index += FS_Size_32();
            xIdx += int32v( FS_Size_32() );

            mask32v xReset = xIdx > xMax;
            yIdx = FS_MaskedIncrement_i32( yIdx, xReset );
            xIdx = FS_MaskedSub_i32( xIdx, xSizeV, xReset );
        }

        float32v xF = FS_Converti32_f32( xIdx ) * xMul;
        float32v yF = FS_Converti32_f32( yIdx ) * yMul;

        float32v xPos = FS_Cos_f32( xF ) * xFreq;
        float32v yPos = FS_Cos_f32( yF ) * yFreq;
        float32v zPos = FS_Sin_f32( xF ) * xFreq;
        float32v wPos = FS_Sin_f32( yF ) * yFreq;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

private:
    static FS_INLINE FastNoise::OutputMinMax DoRemaining( float* noiseOut, size_t totalValues, size_t index, float32v min, float32v max, float32v finalGen )
    {
        FastNoise::OutputMinMax minMax;
        size_t remaining = totalValues - index;

        if( remaining == FS_Size_32() )
        {
            FS_Store_f32( &noiseOut[index], finalGen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, finalGen );
            max = FS_Max_f32( max, finalGen );
#endif
        }
        else
        {
            std::memcpy( &noiseOut[index], &finalGen, remaining * sizeof( int32_t ) );

#if FASTNOISE_CALC_MIN_MAX
            do
            {
                minMax << noiseOut[index];
            }
            while( ++index < totalValues );
#endif
        }

#if FASTNOISE_CALC_MIN_MAX
        float* minP = reinterpret_cast<float*>(&min);
        float* maxP = reinterpret_cast<float*>(&max);
        for( size_t i = 0; i < FS_Size_32(); i++ )
        {
            minMax << FastNoise::OutputMinMax{ minP[i], maxP[i] };
        }
#endif

        return minMax;
    }
};
