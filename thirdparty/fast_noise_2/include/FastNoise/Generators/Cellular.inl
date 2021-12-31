#include "FastSIMD/InlInclude.h"

#include <cfloat>
#include <array>

#include "Cellular.h"
#include "Utils.inl"

template<typename FS>
class FS_T<FastNoise::Cellular, FS> : public virtual FastNoise::Cellular, public FS_T<FastNoise::Generator, FS>
{
protected:
    const float kJitter2D = 0.437016f;
    const float kJitter3D = 0.396144f;
    const float kJitter4D = 0.366025f;
    const float kJitterIdx23 = 0.190983f;
};

template<typename FS>
class FS_T<FastNoise::CellularValue, FS> : public virtual FastNoise::CellularValue, public FS_T<FastNoise::Cellular, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = FnUtils::HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS_Converti32_f32( hash );
                float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd );

                for( int i = 0; ; i++ )
                {
                    mask32v closer = newDistance < distance[i];

                    float32v localDistance = distance[i];
                    float32v localCellValue = value[i];

                    distance[i] = FS_Select_f32( closer, newDistance, distance[i] );
                    value[i] = FS_Select_f32( closer, newCellValue, value[i] );

                    if( i > mValueIndex )
                    {
                        break;
                    }

                    newDistance = FS_Select_f32( closer, localDistance, newDistance );
                    newCellValue = FS_Select_f32( closer, localCellValue, newCellValue );
                }

                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return value[mValueIndex];
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );
        
        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );
        
        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;
    
        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( ( hash >> 10 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( ( hash >> 20 ) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                
                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );
                
                    float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS_Converti32_f32( hash );
                    float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd );
                
                    for( int i = 0; ; i++ )
                    {
                        mask32v closer = newDistance < distance[i];

                        float32v localDistance = distance[i];
                        float32v localCellValue = value[i];

                        distance[i] = FS_Select_f32( closer, newDistance, distance[i] );
                        value[i] = FS_Select_f32( closer, newCellValue, value[i] );

                        if( i > mValueIndex )
                        {
                            break;
                        }

                        newDistance = FS_Select_f32( closer, localDistance, newDistance );
                        newCellValue = FS_Select_f32( closer, localCellValue, newCellValue );
                    }
            
                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }
    
        return value[mValueIndex];
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z , float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );
        std::array<float32v, kMaxDistanceCount> value;
        std::array<float32v, kMaxDistanceCount> distance;
        
        value.fill( float32v( INFINITY ) );
        distance.fill( float32v( INFINITY ) );
        
        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );
        int32v wcBase = FS_Convertf32_i32( w ) + int32v( -1 );
        
        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;
        float32v wcfBase = FS_Converti32_f32( wcBase ) - w;
    
        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );
        wcBase *= int32v( FnPrimes::W );
    
        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS_Converti32_f32( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS_Converti32_f32( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS_Converti32_f32( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS_Converti32_f32( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, FS_FMulAdd_f32( zd, zd, wd * wd ) ) ) );
                        xd = FS_FMulAdd_f32( xd, invMag, xcf );
                        yd = FS_FMulAdd_f32( yd, invMag, ycf );
                        zd = FS_FMulAdd_f32( zd, invMag, zcf );
                        wd = FS_FMulAdd_f32( wd, invMag, wcf );

                        float32v newCellValue = float32v( (float)(1.0 / INT_MAX) ) * FS_Converti32_f32( hash );
                        float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        for( int i = 0; ; i++ )
                        {
                            mask32v closer = newDistance < distance[i];

                            float32v localDistance = distance[i];
                            float32v localCellValue = value[i];

                            distance[i] = FS_Select_f32( closer, newDistance, distance[i] );
                            value[i] = FS_Select_f32( closer, newCellValue, value[i] );

                            if( i > mValueIndex )
                            {
                                break;
                            }

                            newDistance = FS_Select_f32( closer, localDistance, newDistance );
                            newCellValue = FS_Select_f32( closer, localCellValue, newCellValue );
                        }

                        wcf += float32v( 1 );
                        wc += int32v( FnPrimes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }
    
        return value[mValueIndex];
    }
};

template<typename FS>
class FS_T<FastNoise::CellularDistance, FS> : public virtual FastNoise::CellularDistance, public FS_T<FastNoise::Cellular, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for ( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = FnUtils::HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd );

                for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                {
                    distance[i] = FS_Max_f32( FS_Min_f32( distance[i], newDistance ), distance[i - 1] );
                }

                distance[0] = FS_Min_f32( distance[0], newDistance );

                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return GetReturn( distance );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );

                    float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd );

                    for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                    {
                        distance[i] = FS_Max_f32( FS_Min_f32( distance[i], newDistance ), distance[i - 1] );
                    }

                    distance[0] = FS_Min_f32( distance[0], newDistance );

                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return GetReturn( distance );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );

        std::array<float32v, kMaxDistanceCount> distance;
        distance.fill( float32v( INFINITY ) );

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );
        int32v wcBase = FS_Convertf32_i32( w ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;
        float32v wcfBase = FS_Converti32_f32( wcBase ) - w;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );
        wcBase *= int32v( FnPrimes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS_Converti32_f32( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS_Converti32_f32( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS_Converti32_f32( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS_Converti32_f32( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, FS_FMulAdd_f32( zd, zd, wd * wd ) ) ) );
                        xd = FS_FMulAdd_f32( xd, invMag, xcf );
                        yd = FS_FMulAdd_f32( yd, invMag, ycf );
                        zd = FS_FMulAdd_f32( zd, invMag, zcf );
                        wd = FS_FMulAdd_f32( wd, invMag, wcf );

                        float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        for( int i = kMaxDistanceCount - 1; i > 0; i-- )
                        {
                            distance[i] = FS_Max_f32( FS_Min_f32( distance[i], newDistance ), distance[i - 1] );
                        }

                        distance[0] = FS_Min_f32( distance[0], newDistance );

                        wcf += float32v( 1 );
                        wc += int32v( FnPrimes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return GetReturn( distance );
    }

    FS_INLINE float32v GetReturn( std::array<float32v, kMaxDistanceCount>& distance ) const
    {
        if( mDistanceFunction == FastNoise::DistanceFunction::Euclidean )
        {
            distance[mDistanceIndex0] *= FS_InvSqrt_f32( distance[mDistanceIndex0] );
            distance[mDistanceIndex1] *= FS_InvSqrt_f32( distance[mDistanceIndex1] );
        }

        switch( mReturnType )
        {
        default:
        case ReturnType::Index0:
        {
            return distance[mDistanceIndex0];
        }
        case ReturnType::Index0Add1:
        {
            return distance[mDistanceIndex0] + distance[mDistanceIndex1];
        }
        case ReturnType::Index0Sub1:
        {
            return distance[mDistanceIndex0] - distance[mDistanceIndex1];
        }
        case ReturnType::Index0Mul1:
        {
            return distance[mDistanceIndex0] * distance[mDistanceIndex1];
        }
        case ReturnType::Index0Div1:
        {
            return distance[mDistanceIndex0] * FS_Reciprocal_f32( distance[mDistanceIndex1] );
        }
        }
    }
};

template<typename FS>
class FS_T<FastNoise::CellularLookup, FS> : public virtual FastNoise::CellularLookup, public FS_T<FastNoise::Cellular, FS>
{
    FASTSIMD_DECLARE_FS_TYPES;

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y ) const final
    {
        float32v jitter = float32v( this->kJitter2D ) * this->GetSourceValue( mJitterModifier, seed, x, y );
        float32v distance( FLT_MAX );
        float32v cellX, cellY;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                int32v hash = FnUtils::HashPrimesHB( seed, xc, yc );
                float32v xd = FS_Converti32_f32( hash & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );
                float32v yd = FS_Converti32_f32( (hash >> 16) & int32v( 0xffff ) ) - float32v( 0xffff / 2.0f );

                float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, yd * yd ) );
                xd = FS_FMulAdd_f32( xd, invMag, xcf );
                yd = FS_FMulAdd_f32( yd, invMag, ycf );

                float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd );

                mask32v closer = newDistance < distance;
                distance = FS_Min_f32( newDistance, distance );

                cellX = FS_Select_f32( closer, xd + x, cellX );
                cellY = FS_Select_f32( closer, yd + y, cellY );

                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z ) const final
    {
        float32v jitter = float32v( this->kJitter3D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z );
        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc );
                    float32v xd = FS_Converti32_f32( hash & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v yd = FS_Converti32_f32( (hash >> 10) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );
                    float32v zd = FS_Converti32_f32( (hash >> 20) & int32v( 0x3ff ) ) - float32v( 0x3ff / 2.0f );

                    float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, zd * zd ) ) );
                    xd = FS_FMulAdd_f32( xd, invMag, xcf );
                    yd = FS_FMulAdd_f32( yd, invMag, ycf );
                    zd = FS_FMulAdd_f32( zd, invMag, zcf );

                    float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd );

                    mask32v closer = newDistance < distance;
                    distance = FS_Min_f32( newDistance, distance );

                    cellX = FS_Select_f32( closer, xd + x, cellX );
                    cellY = FS_Select_f32( closer, yd + y, cellY );
                    cellZ = FS_Select_f32( closer, zd + z, cellZ );

                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ), cellZ * float32v( mLookupFreq ) );
    }

    float32v FS_VECTORCALL Gen( int32v seed, float32v x, float32v y, float32v z, float32v w ) const final
    {
        float32v jitter = float32v( this->kJitter4D ) * this->GetSourceValue( mJitterModifier, seed, x, y, z, w );
        float32v distance( FLT_MAX );
        float32v cellX, cellY, cellZ, cellW;

        int32v xc = FS_Convertf32_i32( x ) + int32v( -1 );
        int32v ycBase = FS_Convertf32_i32( y ) + int32v( -1 );
        int32v zcBase = FS_Convertf32_i32( z ) + int32v( -1 );
        int32v wcBase = FS_Convertf32_i32( w ) + int32v( -1 );

        float32v xcf = FS_Converti32_f32( xc ) - x;
        float32v ycfBase = FS_Converti32_f32( ycBase ) - y;
        float32v zcfBase = FS_Converti32_f32( zcBase ) - z;
        float32v wcfBase = FS_Converti32_f32( wcBase ) - w;

        xc *= int32v( FnPrimes::X );
        ycBase *= int32v( FnPrimes::Y );
        zcBase *= int32v( FnPrimes::Z );
        wcBase *= int32v( FnPrimes::W );

        for( int xi = 0; xi < 3; xi++ )
        {
            float32v ycf = ycfBase;
            int32v yc = ycBase;
            for( int yi = 0; yi < 3; yi++ )
            {
                float32v zcf = zcfBase;
                int32v zc = zcBase;
                for( int zi = 0; zi < 3; zi++ )
                {
                    float32v wcf = wcfBase;
                    int32v wc = wcBase;
                    for( int wi = 0; wi < 3; wi++ )
                    {
                        int32v hash = FnUtils::HashPrimesHB( seed, xc, yc, zc, wc );
                        float32v xd = FS_Converti32_f32( hash & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v yd = FS_Converti32_f32( (hash >> 8) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v zd = FS_Converti32_f32( (hash >> 16) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );
                        float32v wd = FS_Converti32_f32( (hash >> 24) & int32v( 0xff ) ) - float32v( 0xff / 2.0f );

                        float32v invMag = jitter * FS_InvSqrt_f32( FS_FMulAdd_f32( xd, xd, FS_FMulAdd_f32( yd, yd, FS_FMulAdd_f32( zd, zd, wd * wd ) ) ) );
                        xd = FS_FMulAdd_f32( xd, invMag, xcf );
                        yd = FS_FMulAdd_f32( yd, invMag, ycf );
                        zd = FS_FMulAdd_f32( zd, invMag, zcf );
                        wd = FS_FMulAdd_f32( wd, invMag, wcf );

                        float32v newDistance = FnUtils::CalcDistance( mDistanceFunction, xd, yd, zd, wd );

                        mask32v closer = newDistance < distance;
                        distance = FS_Min_f32( newDistance, distance );

                        cellX = FS_Select_f32( closer, xd + x, cellX );
                        cellY = FS_Select_f32( closer, yd + y, cellY );
                        cellZ = FS_Select_f32( closer, zd + z, cellZ );
                        cellW = FS_Select_f32( closer, wd + w, cellW );

                        wcf += float32v( 1 );
                        wc += int32v( FnPrimes::W );
                    }
                    zcf += float32v( 1 );
                    zc += int32v( FnPrimes::Z );
                }
                ycf += float32v( 1 );
                yc += int32v( FnPrimes::Y );
            }
            xcf += float32v( 1 );
            xc += int32v( FnPrimes::X );
        }

        return this->GetSourceValue( mLookup, seed - int32v( -1 ), cellX * float32v( mLookupFreq ), cellY * float32v( mLookupFreq ), cellZ * float32v( mLookupFreq ), cellW * float32v( mLookupFreq ) );
    }
};
