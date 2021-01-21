#include "FastNoise/FastNoiseMetadata.h"
#include "Base64.h"

#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <cstdint>

using namespace FastNoise;

std::vector<const Metadata*> Metadata::sMetadataClasses;

NodeData::NodeData( const Metadata* data )
{
    metadata = data;

    if( metadata )
    {
        for( const auto& value : metadata->memberVariables )
        {
            variables.push_back( value.valueDefault );
        }

        for( const auto& value : metadata->memberNodes )
        {
            (void)value;
            nodes.push_back( nullptr );
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

bool SerialiseNodeDataInternal( NodeData* nodeData, bool fixUp, std::vector<uint8_t>& dataStream, std::unordered_map<const NodeData*, uint16_t>& referenceIds, std::unordered_set<const NodeData*> dependancies = {} )
{
    const Metadata* metadata = nodeData->metadata;

    if( !metadata ||
        nodeData->variables.size() != metadata->memberVariables.size() ||
        nodeData->nodes.size()     != metadata->memberNodes.size()     ||
        nodeData->hybrids.size()   != metadata->memberHybrids.size()   )
    {
        assert( 0 ); // Member size mismatch with metadata
        return false;
    }

    if( fixUp )
    {
        dependancies.insert( nodeData );

        for( auto& node : nodeData->nodes )
        {
            if( dependancies.find( node ) != dependancies.end() )
            {
                node = nullptr;
            }
        }
        for( auto& hybrid : nodeData->hybrids )
        {
            if( dependancies.find( hybrid.first ) != dependancies.end() )
            {
                hybrid.first = nullptr;
            }
        }
    }

    auto reference = referenceIds.find( nodeData );

    if( reference != referenceIds.end() )
    {
        AddToDataStream( dataStream, UINT16_MAX );
        AddToDataStream( dataStream, reference->second );
        return true;
    }

    AddToDataStream( dataStream, metadata->id );

    for( size_t i = 0; i < metadata->memberVariables.size(); i++ )
    {
        AddToDataStream( dataStream, nodeData->variables[i].i );
    }

    for( size_t i = 0; i < metadata->memberNodes.size(); i++ )
    {
        if( fixUp && nodeData->nodes[i] )
        {
            std::unique_ptr<Generator> gen( metadata->NodeFactory() );
            SmartNode<> node( nodeData->nodes[i]->metadata->NodeFactory() );

            if( !metadata->memberNodes[i].setFunc( gen.get(), node ) )
            {
                nodeData->nodes[i] = nullptr;
                return false;
            }
        }

        if( !nodeData->nodes[i] || !SerialiseNodeDataInternal( nodeData->nodes[i], fixUp, dataStream, referenceIds, dependancies ) )
        {
            return false;
        }
    }

    for( size_t i = 0; i < metadata->memberHybrids.size(); i++ )
    {
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
                std::unique_ptr<Generator> gen( metadata->NodeFactory() );
                std::shared_ptr<Generator> node( nodeData->hybrids[i].first->metadata->NodeFactory() );

                if( !metadata->memberHybrids[i].setNodeFunc( gen.get(), node ) )
                {
                    nodeData->hybrids[i].first = nullptr;
                    return false;
                }
            }

            AddToDataStream( dataStream, (uint8_t)1 );
            if( !SerialiseNodeDataInternal( nodeData->hybrids[i].first, fixUp, dataStream, referenceIds, dependancies ) )
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

SmartNode<> DeserialiseSmartNodeInternal( const std::vector<uint8_t>& serialisedNodeData, size_t& serialIdx, std::unordered_map<uint16_t, SmartNode<>>& referenceNodes, FastSIMD::eLevel level = FastSIMD::Level_Null )
{
    uint16_t nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    if( nodeId == UINT16_MAX )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        auto refNode = referenceNodes.find( referenceId );

        if( refNode == referenceNodes.end() )
        {
            return nullptr;            
        }

        return refNode->second;
    }

    const Metadata* metadata = Metadata::GetMetadataClass( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    SmartNode<> generator( metadata->NodeFactory( level ) );

    for( const auto& var : metadata->memberVariables )
    {
        Metadata::MemberVariable::ValueUnion v;

        if( !GetFromDataStream( serialisedNodeData, serialIdx, v ) )
        {
            return nullptr;
        }

        var.setFunc( generator.get(), v );
    }

    for( const auto& node : metadata->memberNodes )
    {
        SmartNode<> nodeGen = DeserialiseSmartNodeInternal( serialisedNodeData, serialIdx, referenceNodes, level );

        if( !nodeGen || !node.setFunc( generator.get(), nodeGen ) )
        {
            return nullptr;
        }
    }

    for( const auto& hybrid : metadata->memberHybrids )
    {
        uint8_t isGenerator;
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

    referenceNodes.emplace( (uint16_t)referenceNodes.size(), generator );

    return generator;
}

SmartNode<> Metadata::DeserialiseSmartNode( const char* serialisedBase64NodeData, FastSIMD::eLevel level )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    std::unordered_map<uint16_t, SmartNode<>> referenceNodes;

    return DeserialiseSmartNodeInternal( dataStream, startIdx, referenceNodes, level );
}

NodeData* DeserialiseNodeDataInternal( const std::vector<uint8_t>& serialisedNodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut, size_t& serialIdx, std::unordered_map<uint16_t, NodeData*>& referenceNodes )
{
    uint16_t nodeId;
    if( !GetFromDataStream( serialisedNodeData, serialIdx, nodeId ) )
    {
        return nullptr;
    }

    if( nodeId == UINT16_MAX )
    {
        uint16_t referenceId;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, referenceId ) )
        {
            return nullptr;
        }

        auto refNode = referenceNodes.find( referenceId );

        if( refNode == referenceNodes.end() )
        {
            return nullptr;
        }

        return refNode->second;
    }

    const Metadata* metadata = Metadata::GetMetadataClass( nodeId );

    if( !metadata )
    {
        return nullptr;
    }

    std::unique_ptr<NodeData> nodeData( new NodeData( metadata ) );

    for( auto& var : nodeData->variables )
    {
        if( !GetFromDataStream( serialisedNodeData, serialIdx, var ) )
        {
            return nullptr;
        }
    }

    for( auto& node : nodeData->nodes )
    {
        node = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx, referenceNodes );

        if( !node )
        {
            return nullptr;
        }
    }

    for( auto& hybrid : nodeData->hybrids )
    {
        uint8_t isGenerator;
        if( !GetFromDataStream( serialisedNodeData, serialIdx, isGenerator ) || isGenerator > 1 )
        {
            return nullptr;
        }

        if( isGenerator )
        {
            hybrid.first = DeserialiseNodeDataInternal( serialisedNodeData, nodeDataOut, serialIdx, referenceNodes );

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

    referenceNodes.emplace( (uint16_t)referenceNodes.size(), nodeData.get() );

    return nodeDataOut.emplace_back( std::move( nodeData ) ).get();  
}

NodeData* Metadata::DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut )
{
    std::vector<uint8_t> dataStream = Base64::Decode( serialisedBase64NodeData );
    size_t startIdx = 0;

    std::unordered_map<uint16_t, NodeData*> referenceNodes;

    return DeserialiseNodeDataInternal( dataStream, nodeDataOut, startIdx, referenceNodes );
}

#define FASTSIMD_BUILD_CLASS2( CLASS ) \
const CLASS::Metadata g ## CLASS ## Metadata( #CLASS );\
const FastNoise::Metadata* CLASS::GetMetadata() const\
{\
    return &g ## CLASS ## Metadata;\
}\
Generator* CLASS::Metadata::NodeFactory( FastSIMD::eLevel l ) const\
{\
    return FastSIMD::New<CLASS>( l );\
}

#define FASTSIMD_BUILD_CLASS( CLASS ) FASTSIMD_BUILD_CLASS2( CLASS )

#define FASTNOISE_CLASS( CLASS ) CLASS

#define FASTSIMD_INCLUDE_HEADER_ONLY
#include "FastNoise/FastNoise_BuildList.inl"