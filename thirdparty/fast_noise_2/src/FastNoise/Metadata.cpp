#define FASTNOISE_METADATA // Should only be defined here

#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <limits>
#include <cassert>
#include <cstdint>

#include "FastNoise/Metadata.h"
#include "FastNoise/FastNoise.h"
#include "Base64.h"

using namespace FastNoise;

std::vector<const Metadata*> Metadata::sAllMetadata;

NodeData::NodeData( const Metadata* data )
{
    metadata = data;

    if( metadata )
    {
        for( const auto& value : metadata->memberVariables )
        {
            variables.push_back( value.valueDefault );
        }

        for( const auto& value : metadata->memberNodeLookups )
        {
            (void)value;
            nodeLookups.push_back( nullptr );
        }

        for( const auto& value : metadata->memberHybrids )
        {
            hybrids.emplace_back( nullptr, value.valueDefault );
        }
    }
}

template<typename T>
void AddToDataStream( std::vector<uint8_t>& dataStream, T value )
{
    for( size_t i = 0; i < sizeof( T ); i++ )
    {
        dataStream.push_back( (uint8_t)(value >> (i * 8)) );        
    }
}

bool SerialiseNodeDataInternal( NodeData* nodeData, bool fixUp, std::vector<uint8_t>& dataStream, std::unordered_map<const NodeData*, uint16_t>& referenceIds, std::unordered_set<const NodeData*> dependencies = {} )
{
    // dependencies passed by value to avoid false positives from other branches in the node tree

    const Metadata* metadata = nodeData->metadata;

    if( !metadata ||
        nodeData->variables.size() != metadata->memberVariables.size()   ||
        nodeData->nodeLookups.size()     != metadata->memberNodeLookups.size() ||
        nodeData->hybrids.size()   != metadata->memberHybrids.size()     )
    {
        assert( 0 ); // Member size mismatch with metadata
        return false;
    }

    if( fixUp )
    {
        dependencies.insert( nodeData );

        // Null any dependency loops 
        for( auto& node : nodeData->nodeLookups )
        {
            if( dependencies.find( node ) != dependencies.end() )
            {
                node = nullptr;
            }
        }
        for( auto& hybrid : nodeData->hybrids )
        {
            if( dependencies.find( hybrid.first ) != dependencies.end() )
            {
                hybrid.first = nullptr;
            }
        }
    }

    // Reference previously encoded nodes to reduce encoded string length
    // Relevant if a node has multiple links from it's output
    auto reference = referenceIds.find( nodeData );

    if( reference != referenceIds.end() )
    {
        // UINT16_MAX where node ID should be
        // Referenced by index in reference array, array ordering will match on decode
        AddToDataStream( dataStream, std::numeric_limits<uint16_t>::max() );
        AddToDataStream( dataStream, reference->second );
        return true;
    }

    // Node ID
    AddToDataStream( dataStream, metadata->id );

    // Member variables
    for( size_t i = 0; i < metadata->memberVariables.size(); i++ )
    {
        AddToDataStream( dataStream, nodeData->variables[i].i );
    }

    // Member nodes
    for( size_t i = 0; i < metadata->memberNodeLookups.size(); i++ )
    {
        if( fixUp && nodeData->nodeLookups[i] )
        {
            // Create test node to see if source is a valid node type
            SmartNode<> test = metadata->CreateNode();
            SmartNode<> node = nodeData->nodeLookups[i]->metadata->CreateNode();

            if( !metadata->memberNodeLookups[i].setFunc( test.get(), node ) )
            {
                nodeData->nodeLookups[i] = nullptr;
                return false;
            }
        }

        if( !nodeData->nodeLookups[i] || !SerialiseNodeDataInternal( nodeData->nodeLookups[i], fixUp, dataStream, referenceIds, dependencies ) )
        {
            return false;
        }
    }

    // Member hybrids
    for( size_t i = 0; i < metadata->memberHybrids.size(); i++ )
    {
        // 1 byte to indicate:
        // 0 = constant float value
        // 1 = node lookup

        if( !nodeData->hybrids[i].first )
        {
            AddToDataStream( dataStream, (uint8_t)0 );

            Metadata::MemberVariable::ValueUnion v = nodeData->hybrids[i].second;

            AddToDataStream( dataStream, v.i );
        }
        else
        {
            if( fixUp )
            {
                // Create test node to see if source is a valid node type
                SmartNode<> test = metadata->CreateNode();
                SmartNode<> node = nodeData->hybrids[i].first->metadata->CreateNode();

                if( !metadata->memberHybrids[i].setNodeFunc( test.get(), node ) )
                {
                    nodeData->hybrids[i].first = nullptr;
                    return false;
                }
            }

            AddToDataStream( dataStream, (uint8_t)1 );
            if( !SerialiseNodeDataInternal( nodeData->hybrids[i].first, fixUp, dataStream, referenceIds, dependencies ) )
            {
                return false;
            }
        }
    }

    referenceIds.emplace( nodeData, (uint16_t)referenceIds.size() );

    return true; 
}

std::string Metadata::SerialiseNodeData( NodeData* nodeData, bool fixUp )
{
    std::vector<uint8_t> serialData;
    std::unordered_map<const NodeData*, uint16_t> referenceIds;

    if( !SerialiseNodeDataInternal( nodeData, fixUp, serialData, referenceIds ) )
    {
        return "";
    }
    return Base64::Encode( serialData );
}

template<typename T>
bool GetFromDataStream( const std::vector<uint8_t>& dataStream, size_t& idx, T& value )
{
    if( dataStream.size() < idx + sizeof( T ) )
    {
        return false;
    }

    value = *reinterpret_cast<const T*>( dataStream.data() + idx );

    idx += sizeof( T );
    return true;
}

SmartNode<> DeserialiseSmartNodeInternal( const std::vector<uint8_t>& serialisedNodeData, size_t& serialIdx, std::vector<SmartNode<>>& referenceNodes, FastSIMD::eLevel level = FastSIMD::Level_Null )
{
    uint16_t nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    // UINT16_MAX indicates a reference node
    if( nodeId == std::numeric_limits<uint16_t>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        if( referenceId >= referenceNodes.size() )
        {
            return nullptr;            
        }

        return referenceNodes[referenceId];
    }

    // Create node from nodeId
    const Metadata* metadata = Metadata::GetFromId( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    SmartNode<> generator = metadata->CreateNode( level );

    if( !generator )
    {
        return nullptr;
    }

    // Member variables
    for( const auto& var : metadata->memberVariables )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, serialIdx, v.i ) )
        {
            return nullptr;
        }

        var.setFunc( generator.get(), v );
    }

    // Member nodes
    for( const auto& node : metadata->memberNodeLookups )
    {
        SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level );

        if( !nodeGen || !node.setFunc( generator.get(), nodeGen ) )
        {
            return nullptr;
        }
    }

    // Member variables
    for( const auto& hybrid : metadata->memberHybrids )
    {
        uint8_t isGenerator;
        // 1 byte to indicate:
        // 0 = constant float value
        // 1 = node lookup

        if( !GetFromDataStream( serialisedNodeData, serialIdx, isGenerator ) || isGenerator > 1 )
        {
            return nullptr;
        }

        if( isGenerator )
        {
            SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level );

            if( !nodeGen || !hybrid.setNodeFunc( generator.get(), nodeGen ) )
            {
                return nullptr;
            }
        }
        else
        {
            float v;

            if( !GetFromDataStream( serialisedNodeData, serialIdx, v ) )
            {
                return nullptr;
            }

            hybrid.setValueFunc( generator.get(), v );
        }
    }

    referenceNodes.emplace_back( generator );

    return generator;
}

SmartNode<> FastNoise::NewFromEncodedNodeTree( const char* serialisedBase64NodeData, FastSIMD::eLevel level )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    std::vector<SmartNode<>> referenceNodes;

    return DeserialiseSmartNodeInternal( dataStream, startIdx, referenceNodes, level );
}

NodeData* DeserialiseNodeDataInternal( const std::vector<uint8_t>& serialisedNodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut, size_t& serialIdx )
{
    uint16_t nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    // UINT16_MAX indicates a reference node
    if( nodeId == std::numeric_limits<uint16_t>::max() )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        if( referenceId >= nodeDataOut.size() )
        {
            return nullptr;
        }

        return nodeDataOut[referenceId].get();
    }

    // Create node from nodeId
    const Metadata* metadata = Metadata::GetFromId( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    std::unique_ptr<NodeData> nodeData( new NodeData( metadata ) );

    // Member variables
    for( auto& var : nodeData->variables )
    {
        if( !GetFromDataStream( serialisedNodeData, serialIdx, var ) )
        {
            return nullptr;
        }
    }

    // Member nodes
    for( auto& node : nodeData->nodeLookups )
    {
        node = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx );

        if( !node )
        {
            return nullptr;
        }
    }

    // Member hybrids
    for( auto& hybrid : nodeData->hybrids )
    {
        uint8_t isGenerator;
        // 1 byte to indicate:
        // 0 = constant float value
        // 1 = node lookup

        if( !GetFromDataStream( serialisedNodeData, serialIdx, isGenerator ) || isGenerator > 1 )
        {
            return nullptr;
        }

        if( isGenerator )
        {
            hybrid.first = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx );

            if( !hybrid.first )
            {
                return nullptr;
            }
        }
        else
        {
            if( !GetFromDataStream( serialisedNodeData, serialIdx, hybrid.second ) )
            {
                return nullptr;
            }
        }
    }

    return nodeDataOut.emplace_back( std::move( nodeData ) ).get();  
}

NodeData* Metadata::DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    return DeserialiseNodeDataInternal( dataStream, nodeDataOut, startIdx );
}

std::string Metadata::FormatMetadataNodeName( const Metadata* metadata, bool removeGroups )
{
    std::string string = metadata->name;
    for( size_t i = 1; i < string.size(); i++ )
    {
        if( ( isdigit( string[i] ) || isupper( string[i] ) ) && islower( string[i - 1] ) )
        {
            string.insert( i++, 1, ' ' );
        }
    }

    if( removeGroups )
    {
        for( auto group : metadata->groups )
        {
            size_t start_pos = string.find( group );
            if( start_pos != std::string::npos )
            {
                string.erase( start_pos, std::strlen( group ) + 1 );
            }
        }
    }
    return string;
}

std::string Metadata::FormatMetadataMemberName( const Member& member )
{
    std::string string = member.name;
    if( member.dimensionIdx >= 0 )
    {
        string.insert( string.begin(), ' ' );
        string.insert( 0, kDim_Strings[member.dimensionIdx] );
    }
    return string;
}

namespace FastNoise
{
    template<typename T>
    struct MetadataT;
}

template<typename T>
std::unique_ptr<const MetadataT<T>> CreateMetadataInstance( const char* className )
{
    auto* newMetadata = new MetadataT<T>;
    newMetadata->name = className;
    return std::unique_ptr<const MetadataT<T>>( newMetadata );
}

#if FASTNOISE_USE_SHARED_PTR
#define FASTNOISE_GET_MEMORY_ALLOCATOR()
#else
#define FASTNOISE_GET_MEMORY_ALLOCATOR() , &SmartNodeManager::Allocate
#endif

#define FASTSIMD_BUILD_CLASS2( CLASS ) \
const std::unique_ptr<const FastNoise::MetadataT<CLASS>> g ## CLASS ## Metadata = CreateMetadataInstance<CLASS>( #CLASS );\
template<> FASTNOISE_API const FastNoise::Metadata& FastNoise::Impl::GetMetadata<CLASS>()\
{\
    return *g ## CLASS ## Metadata;\
}\
const FastNoise::Metadata& CLASS::GetMetadata() const\
{\
    return FastNoise::Impl::GetMetadata<CLASS>();\
}\
SmartNode<> FastNoise::MetadataT<CLASS>::CreateNode( FastSIMD::eLevel l ) const\
{\
    return SmartNode<>( FastSIMD::New<CLASS>( l FASTNOISE_GET_MEMORY_ALLOCATOR() ) );\
}

#define FASTSIMD_BUILD_CLASS( CLASS ) FASTSIMD_BUILD_CLASS2( CLASS )

#define FASTNOISE_CLASS( CLASS ) CLASS

#define FASTSIMD_INCLUDE_HEADER_ONLY
#include "FastNoise/FastNoise_BuildList.inl"