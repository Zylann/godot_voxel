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

    using VoidPtrStorageType = const FS_T<Generator, FS>*;

    void SetSourceSIMDPtr( const Generator* base, const void** simdPtr ) final
    {
        if( !base )
        {
            *simdPtr = nullptr;
            return;
        }
        auto simd = dynamic_cast<VoidPtrStorageType>( base );

        assert( simd );
        *simdPtr = reinterpret_cast<const void*>( simd );
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

        auto simdT = static_cast<const FS_T<T, FS>*>( simdGen );
        return simdT;
    }

    FastNoise::OutputMinMax GenUniformGrid2D( float* noiseOut, int xStart, int yStart, int xSize, int ySize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize;
        intptr_t index = 0;

        xIdx += int32v::FS_Incremented();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        while( index < totalValues - (intptr_t)FS_Size_32() )
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

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
        }

        float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
        float32v yPos = FS_Converti32_f32( yIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid3D( float* noiseOut, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float frequency, int seed ) const final
    {
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

        intptr_t totalValues = xSize * ySize * zSize;
        intptr_t index = 0;

        xIdx += int32v::FS_Incremented();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );
        AxisReset<true>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );

        while( index < totalValues - (intptr_t)FS_Size_32() )
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
            
            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            AxisReset<false>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
        }

        float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
        float32v yPos = FS_Converti32_f32( yIdx ) * freqV;
        float32v zPos = FS_Converti32_f32( zIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenUniformGrid4D( float* noiseOut, int xStart, int yStart, int zStart, int wStart, int xSize, int ySize, int zSize, int wSize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( xStart );
        int32v yIdx( yStart );
        int32v zIdx( zStart );
        int32v wIdx( wStart );

        float32v freqV( frequency );

        int32v xSizeV( xSize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );
        int32v ySizeV( ySize );
        int32v yMax = ySizeV + yIdx + int32v( -1 );
        int32v zSizeV( zSize );
        int32v zMax = zSizeV + zIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize * zSize * wSize;
        intptr_t index = 0;

        xIdx += int32v::FS_Incremented();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );
        AxisReset<true>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
        AxisReset<true>( zIdx, wIdx, zMax, zSizeV, xSize * ySize * zSize );

        while( index < totalValues - (intptr_t)FS_Size_32() )
        {
            float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
            float32v yPos = FS_Converti32_f32( yIdx ) * freqV;
            float32v zPos = FS_Converti32_f32( zIdx ) * freqV;
            float32v wPos = FS_Converti32_f32( wIdx ) * freqV;

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
            FS_Store_f32( &noiseOut[index], gen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, gen );
            max = FS_Max_f32( max, gen );
#endif

            index += FS_Size_32();
            xIdx += int32v( FS_Size_32() );

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
            AxisReset<false>( yIdx, zIdx, yMax, ySizeV, xSize * ySize );
            AxisReset<false>( zIdx, wIdx, zMax, zSizeV, xSize * ySize * zSize );
        }

        float32v xPos = FS_Converti32_f32( xIdx ) * freqV;
        float32v yPos = FS_Converti32_f32( yIdx ) * freqV;
        float32v zPos = FS_Converti32_f32( zIdx ) * freqV;
        float32v wPos = FS_Converti32_f32( wIdx ) * freqV;

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, totalValues, index, min, max, gen );
    }

    FastNoise::OutputMinMax GenPositionArray2D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)FS_Size_32() )
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

    FastNoise::OutputMinMax GenPositionArray3D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)FS_Size_32() )
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

    FastNoise::OutputMinMax GenPositionArray4D( float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        intptr_t index = 0;
        while( index < count - (intptr_t)FS_Size_32() )
        {
            float32v xPos = float32v( xOffset ) + FS_Load_f32( &xPosArray[index] );
            float32v yPos = float32v( yOffset ) + FS_Load_f32( &yPosArray[index] );
            float32v zPos = float32v( zOffset ) + FS_Load_f32( &zPosArray[index] );
            float32v wPos = float32v( wOffset ) + FS_Load_f32( &wPosArray[index] );

            float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );
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
        float32v wPos = float32v( wOffset ) + FS_Load_f32( &wPosArray[index] );

        float32v gen = Gen( int32v( seed ), xPos, yPos, zPos, wPos );

        return DoRemaining( noiseOut, count, index, min, max, gen );
    }

    float GenSingle2D( float x, float y, int seed ) const final
    {
        return FS_Extract0_f32( Gen( int32v( seed ), float32v( x ), float32v( y ) ) );
    }

    float GenSingle3D( float x, float y, float z, int seed ) const final
    {
        return FS_Extract0_f32( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ) ) );
    }

    float GenSingle4D( float x, float y, float z, float w, int seed ) const final
    {
        return FS_Extract0_f32( Gen( int32v( seed ), float32v( x ), float32v( y ), float32v( z ), float32v( w ) ) );
    }

    FastNoise::OutputMinMax GenTileable2D( float* noiseOut, int xSize, int ySize, float frequency, int seed ) const final
    {
        float32v min( INFINITY );
        float32v max( -INFINITY );

        int32v xIdx( 0 );
        int32v yIdx( 0 );

        int32v xSizeV( xSize );
        int32v ySizeV( ySize );
        int32v xMax = xSizeV + xIdx + int32v( -1 );

        intptr_t totalValues = xSize * ySize;
        intptr_t index = 0;

        float pi2Recip( 0.15915493667f );
        float xSizePi = (float)xSize * pi2Recip;
        float ySizePi = (float)ySize * pi2Recip;
        float32v xFreq = float32v( frequency * xSizePi );
        float32v yFreq = float32v( frequency * ySizePi );
        float32v xMul = float32v( 1 / xSizePi );
        float32v yMul = float32v( 1 / ySizePi );

        xIdx += int32v::FS_Incremented();

        AxisReset<true>( xIdx, yIdx, xMax, xSizeV, xSize );

        while( index < totalValues - (intptr_t)FS_Size_32() )
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

            AxisReset<false>( xIdx, yIdx, xMax, xSizeV, xSize );
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
    template<bool INITIAL>
    static FS_INLINE void AxisReset( int32v& aIdx, int32v& bIdx, int32v aMax, int32v aSize, size_t aStep )
    {
        for( size_t resetLoop = INITIAL ? aStep : 0; resetLoop < FS_Size_32(); resetLoop += aStep )
        {
            mask32v aReset = aIdx > aMax;
            bIdx = FS_MaskedIncrement_i32( bIdx, aReset );
            aIdx = FS_MaskedSub_i32( aIdx, aSize, aReset );
        }
    }

    static FS_INLINE FastNoise::OutputMinMax DoRemaining( float* noiseOut, intptr_t totalValues, intptr_t index, float32v min, float32v max, float32v finalGen )
    {
        FastNoise::OutputMinMax minMax;
        intptr_t remaining = totalValues - index;

        if( remaining == (intptr_t)FS_Size_32() )
        {
            FS_Store_f32( &noiseOut[index], finalGen );

#if FASTNOISE_CALC_MIN_MAX
            min = FS_Min_f32( min, finalGen );
            max = FS_Max_f32( max, finalGen );
#endif
        }
        else
        {
            std::memcpy( &noiseOut[index], &finalGen, remaining * sizeof( float ) );

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
