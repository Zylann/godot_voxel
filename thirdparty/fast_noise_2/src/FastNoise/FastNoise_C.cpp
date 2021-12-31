#include <FastNoise/FastNoise_C.h>
#include <FastNoise/FastNoise.h>
#include <FastNoise/Metadata.h>

FastNoise::Generator* ToGen( void* p )
{
    return static_cast<FastNoise::SmartNode<>*>( p )->get();
}

const FastNoise::Generator* ToGen( const void* p )
{
    return static_cast<const FastNoise::SmartNode<>*>( p )->get();
}

void StoreMinMax( float* floatArray2, FastNoise::OutputMinMax minMax )
{
    if( floatArray2 )
    {
        floatArray2[0] = minMax.min;
        floatArray2[1] = minMax.max;
    }
}

void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned simdLevel )
{
    if( FastNoise::SmartNode<> node = FastNoise::NewFromEncodedNodeTree( encodedString, (FastSIMD::eLevel)simdLevel ) )
    {
        return new FastNoise::SmartNode<>( std::move( node ) );
    }
    return nullptr;
}

void fnDeleteNodeRef( void* node )
{
    delete static_cast<FastNoise::SmartNode<>*>( node );
}

unsigned fnGetSIMDLevel( const void* node )
{
    return (unsigned)ToGen( node )->GetSIMDLevel();
}

int fnGetMetadataID( const void* node )
{
    return ToGen( node )->GetMetadata().id;
}

void fnGenUniformGrid2D( const void* node, float* noiseOut, int xStart, int yStart, int xSize, int ySize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid2D( noiseOut, xStart, yStart, xSize, ySize, frequency, seed ) );    
}

void fnGenUniformGrid3D( const void* node, float* noiseOut, int xStart, int yStart, int zStart, int xSize, int ySize, int zSize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid3D( noiseOut, xStart, yStart, zStart, xSize, ySize, zSize, frequency, seed ) );    
}

void fnGenUniformGrid4D( const void* node, float* noiseOut, int xStart, int yStart, int zStart, int wStart, int xSize, int ySize, int zSize, int wSize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenUniformGrid4D( noiseOut, xStart, yStart, zStart, wStart, xSize, ySize, zSize, wSize, frequency, seed ) );    
}

void fnGenPositionArray2D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, float xOffset, float yOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray2D( noiseOut, count, xPosArray, yPosArray, xOffset, yOffset, seed ) );
}

void fnGenPositionArray3D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, float xOffset, float yOffset, float zOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray3D( noiseOut, count, xPosArray, yPosArray, zPosArray, xOffset, yOffset, zOffset, seed ) );
}

void fnGenPositionArray4D( const void* node, float* noiseOut, int count, const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray, float xOffset, float yOffset, float zOffset, float wOffset, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenPositionArray4D( noiseOut, count, xPosArray, yPosArray, zPosArray, wPosArray, xOffset, yOffset, zOffset, wOffset, seed ) );
}

float fnGenSingle2D( const void* node, float x, float y, int seed )
{
    return ToGen( node )->GenSingle2D( x, y, seed );
}

float fnGenSingle3D( const void* node, float x, float y, float z, int seed )
{
    return ToGen( node )->GenSingle3D( x, y, z, seed );
}

float fnGenSingle4D( const void* node, float x, float y, float z, float w, int seed )
{
    return ToGen( node )->GenSingle4D( x, y, z, w, seed );
}

void fnGenTileable2D( const void* node, float* noiseOut, int xSize, int ySize, float frequency, int seed, float* outputMinMax )
{
    StoreMinMax( outputMinMax, ToGen( node )->GenTileable2D( noiseOut, xSize, ySize, frequency, seed ) );
}

int fnGetMetadataCount()
{
    return (int)FastNoise::Metadata::GetAll().size();
}

const char* fnGetMetadataName( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        return metadata->name;
    }
    return "INVALID NODE ID";
}

void* fnNewFromMetadata( int id, unsigned simdLevel )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        return new FastNoise::SmartNode<>( metadata->CreateNode( (FastSIMD::eLevel)simdLevel ) );
    }
    return nullptr;
}

int fnGetMetadataVariableCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        return (int)metadata->memberVariables.size();
    }
    return -1;
}

const char* fnGetMetadataVariableName( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return metadata->memberVariables[variableIndex].name;
        }
        return "INVALID VARIABLE INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataVariableType( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return (int)metadata->memberVariables[variableIndex].type;
        }
        return -1;
    }
    return -1;
}

int fnGetMetadataVariableDimensionIdx( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return metadata->memberVariables[variableIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

int fnGetMetadataEnumCount( int id, int variableIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            return (int)metadata->memberVariables[variableIndex].enumNames.size();
        }
        return -1;
    }
    return -1;
}

const char* fnGetMetadataEnumName( int id, int variableIndex, int enumIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)variableIndex < metadata->memberVariables.size() )
        {
            if( (size_t)enumIndex < metadata->memberVariables[variableIndex].enumNames.size() )
            {
                return metadata->memberVariables[variableIndex].enumNames[enumIndex];
            }
            return "INVALID ENUM INDEX";
        }
        return "INVALID VARIABLE INDEX";
    }
    return "INVALID NODE ID";
}

bool fnSetVariableFloat( void* node, int variableIndex, float value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)variableIndex < metadata.memberVariables.size() )
    {
        return metadata.memberVariables[variableIndex].setFunc( ToGen( node ), value );
    }
    return false;
}

bool fnSetVariableIntEnum( void* node, int variableIndex, int value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)variableIndex < metadata.memberVariables.size() )
    {
        return metadata.memberVariables[variableIndex].setFunc( ToGen( node ), value );
    }
    return false;
}

int fnGetMetadataNodeLookupCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        return (int)metadata->memberNodeLookups.size();
    }
    return -1;
}

const char* fnGetMetadataNodeLookupName( int id, int nodeLookupIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)nodeLookupIndex < metadata->memberNodeLookups.size() )
        {
            return metadata->memberNodeLookups[nodeLookupIndex].name;
        }
        return "INVALID NODE LOOKUP INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataNodeLookupDimensionIdx( int id, int nodeLookupIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)nodeLookupIndex < metadata->memberNodeLookups.size() )
        {
            return metadata->memberNodeLookups[nodeLookupIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

bool fnSetNodeLookup( void* node, int nodeLookupIndex, const void* nodeLookup )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)nodeLookupIndex < metadata.memberNodeLookups.size() )
    {
        return metadata.memberNodeLookups[nodeLookupIndex].setFunc( ToGen( node ), *static_cast<const FastNoise::SmartNode<>*>( nodeLookup ) );
    }
    return false;
}

int fnGetMetadataHybridCount( int id )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        return (int)metadata->memberHybrids.size();
    }
    return -1;
}

const char* fnGetMetadataHybridName( int id, int hybridIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)hybridIndex < metadata->memberHybrids.size() )
        {
            return metadata->memberHybrids[hybridIndex].name;
        }
        return "INVALID HYBRID INDEX";
    }
    return "INVALID NODE ID";
}

int fnGetMetadataHybridDimensionIdx( int id, int hybridIndex )
{
    if( const FastNoise::Metadata* metadata = FastNoise::Metadata::GetFromId( (uint16_t)id ) )
    {
        if( (size_t)hybridIndex < metadata->memberHybrids.size() )
        {
            return metadata->memberHybrids[hybridIndex].dimensionIdx;
        }
        return -1;
    }
    return -1;
}

bool fnSetHybridNodeLookup( void* node, int hybridIndex, const void* nodeLookup )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)hybridIndex < metadata.memberHybrids.size() )
    {
        return metadata.memberHybrids[hybridIndex].setNodeFunc( ToGen( node ), *static_cast<const FastNoise::SmartNode<>*>( nodeLookup ) );
    }
    return false;
}

bool fnSetHybridFloat( void* node, int hybridIndex, float value )
{
    const FastNoise::Metadata& metadata = ToGen( node )->GetMetadata();
    if( (size_t)hybridIndex < metadata.memberHybrids.size() )
    {
        return metadata.memberHybrids[hybridIndex].setValueFunc( ToGen( node ), value );
    }
    return false;
}
