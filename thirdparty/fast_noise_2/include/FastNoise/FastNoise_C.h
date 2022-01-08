#ifndef FASTNOISE_C_H
#define FASTNOISE_C_H

#include "FastNoise_Export.h"

#ifdef __cplusplus
extern "C" {
#endif

FASTNOISE_API void* fnNewFromEncodedNodeTree( const char* encodedString, unsigned /*FastSIMD::eLevel*/ simdLevel /*0 = Auto*/ );

FASTNOISE_API void fnDeleteNodeRef( void* node );

FASTNOISE_API unsigned fnGetSIMDLevel( const void* node );
FASTNOISE_API int fnGetMetadataID( const void* node );

FASTNOISE_API void fnGenUniformGrid2D( const void* node, float* noiseOut,
                                       int xStart, int yStart,
                                       int xSize, int ySize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenUniformGrid3D( const void* node, float* noiseOut,
                                       int xStart, int yStart, int zStart,
                                       int xSize, int ySize, int zSize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenUniformGrid4D( const void* node, float* noiseOut,
                                       int xStart, int yStart, int zStart, int wStart,
                                       int xSize, int ySize, int zSize, int wSize,
                                       float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray2D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray,
                                         float xOffset, float yOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray3D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray,
                                         float xOffset, float yOffset, float zOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenPositionArray4D( const void* node, float* noiseOut, int count,
                                         const float* xPosArray, const float* yPosArray, const float* zPosArray, const float* wPosArray,
                                         float xOffset, float yOffset, float zOffset, float wOffset,
                                         int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API void fnGenTileable2D( const void* node, float* noiseOut,
                                    int xSize, int ySize,
                                    float frequency, int seed, float* outputMinMax /*nullptr or float[2]*/ );

FASTNOISE_API float fnGenSingle2D( const void* node, float x, float y, int seed );
FASTNOISE_API float fnGenSingle3D( const void* node, float x, float y, float z, int seed );
FASTNOISE_API float fnGenSingle4D( const void* node, float x, float y, float z, float w, int seed );

FASTNOISE_API int fnGetMetadataCount();
FASTNOISE_API const char* fnGetMetadataName( int id ); // valid IDs up to `fnGetMetadataCount() - 1`
FASTNOISE_API void* fnNewFromMetadata( int id, unsigned /*FastSIMD::eLevel*/ simdLevel /*0 = Auto*/ );

FASTNOISE_API int fnGetMetadataVariableCount( int id );
FASTNOISE_API const char* fnGetMetadataVariableName( int id, int variableIndex );
FASTNOISE_API int fnGetMetadataVariableType( int id, int variableIndex );
FASTNOISE_API int fnGetMetadataVariableDimensionIdx( int id, int variableIndex );
FASTNOISE_API int fnGetMetadataEnumCount( int id, int variableIndex );
FASTNOISE_API const char* fnGetMetadataEnumName( int id, int variableIndex, int enumIndex );
FASTNOISE_API bool fnSetVariableFloat( void* node, int variableIndex, float value );
FASTNOISE_API bool fnSetVariableIntEnum( void* node, int variableIndex, int value );

FASTNOISE_API int fnGetMetadataNodeLookupCount( int id ); 
FASTNOISE_API const char* fnGetMetadataNodeLookupName( int id, int nodeLookupIndex );
FASTNOISE_API int fnGetMetadataNodeLookupDimensionIdx( int id, int nodeLookupIndex );
FASTNOISE_API bool fnSetNodeLookup( void* node, int nodeLookupIndex, const void* nodeLookup );

FASTNOISE_API int fnGetMetadataHybridCount( int id );
FASTNOISE_API const char* fnGetMetadataHybridName( int id, int hybridIndex );
FASTNOISE_API int fnGetMetadataHybridDimensionIdx( int id, int hybridIndex );
FASTNOISE_API bool fnSetHybridNodeLookup( void* node, int hybridIndex, const void* nodeLookup );
FASTNOISE_API bool fnSetHybridFloat( void* node, int hybridIndex, float value );

#ifdef __cplusplus
}
#endif

#endif
