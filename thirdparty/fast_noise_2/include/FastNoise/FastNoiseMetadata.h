#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <vector>
#include <cstdint>

#include "FastNoise_Config.h"
#include "FastSIMD/FastSIMD.h"

namespace FastNoise
{
    class Generator;
    template<typename T>
    struct PerDimensionVariable;
    struct NodeData;

    struct Metadata
    {
        Metadata( const char* className )
        {
            name = className;
            id = AddMetadataClass( this );
        }

        static const std::vector<const Metadata*>& GetMetadataClasses()
        {
            return sMetadataClasses;
        }

        static const Metadata* GetMetadataClass( std::uint16_t nodeId )
        {
            if( nodeId < sMetadataClasses.size() )
            {
                return sMetadataClasses[nodeId];
            }

            return nullptr;
        }

        static std::string SerialiseNodeData( NodeData* nodeData, bool fixUp = false );
        static SmartNode<> DeserialiseSmartNode( const char* serialisedBase64NodeData, FastSIMD::eLevel level = FastSIMD::Level_Null );
        static NodeData* DeserialiseNodeData( const char* serialisedBase64NodeData, std::vector<std::unique_ptr<NodeData>>& nodeDataOut );

        struct MemberVariable
        {
            enum eType
            {
                EFloat,
                EInt,
                EEnum
            };

            union ValueUnion
            {
                float f;
                std::int32_t i;

                ValueUnion( float v = 0 )
                {
                    f = v;
                }

                ValueUnion( std::int32_t v )
                {
                    i = v;
                }

                operator float()
                {
                    return f;
                }

                operator std::int32_t()
                {
                    return i;
                }

                bool operator ==( const ValueUnion& rhs ) const
                {
                    return i == rhs.i;
                }
            };

            const char* name;
            eType type;
            int dimensionIdx = -1;
            ValueUnion valueDefault, valueMin, valueMax;
            std::vector<const char*> enumNames;

            std::function<void( Generator*, ValueUnion )> setFunc;
        };

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, U&& func, T minV = 0, T maxV = 0 )
        {
            MemberVariable member;
            member.name = name;
            member.valueDefault = defaultV;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) { func( dynamic_cast<GetArg<U, 0>>(g), v ); };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddVariable( const char* name, T defaultV, void(U::* func)(T), T minV = 0, T maxV = 0 )
        {
            MemberVariable member;
            member.name = name;
            member.valueDefault = defaultV;
            member.valueMin = minV;
            member.valueMax = maxV;

            member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) { (dynamic_cast<U*>(g)->*func)(v); };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<std::is_enum_v<T>>, typename... NAMES>
        void AddVariableEnum( const char* name, T defaultV, void(U::* func)(T), NAMES... names )
        {
            MemberVariable member;
            member.name = name;
            member.type = MemberVariable::EEnum;
            member.valueDefault = (int32_t)defaultV;
            member.enumNames = { names... };

            member.setFunc = [func]( Generator* g, MemberVariable::ValueUnion v ) { (dynamic_cast<U*>(g)->*func)((T)v.i); };

            memberVariables.push_back( member );
        }

        template<typename T, typename U, typename = std::enable_if_t<!std::is_enum_v<T>>>
        void AddPerDimensionVariable( const char* name, T defaultV, U&& func, T minV = 0, T maxV = 0 )
        {
            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<T>::varArray ) / sizeof( *PerDimensionVariable<T>::varArray ); idx++ )
            {
                MemberVariable member;
                member.name = name;
                member.valueDefault = defaultV;
                member.valueMin = minV;
                member.valueMax = maxV;

                member.type = std::is_same_v<T, float> ? MemberVariable::EFloat : MemberVariable::EInt;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( Generator* g, MemberVariable::ValueUnion v ) { func( dynamic_cast<GetArg<U, 0>>(g) ).get()[idx] = v; };

                memberVariables.push_back( member );
            }
        }

        struct MemberNode
        {
            const char* name;
            int dimensionIdx = -1;

            std::function<bool( Generator*, SmartNodeArg<> )> setFunc;
        };

        template<typename T, typename U>
        void AddGeneratorSource( const char* name, void(U::* func)(SmartNodeArg<T>) )
        {
            MemberNode member;
            member.name = name;

            member.setFunc = [func]( Generator* g, SmartNodeArg<> s )
            {
                SmartNode<T> downCast = std::dynamic_pointer_cast<T>(s);
                if( downCast )
                {
                    (dynamic_cast<U*>(g)->*func)( downCast );
                }
                return (bool)downCast;
            };

            memberNodes.push_back( member );
        }

        template<typename U>
        void AddPerDimensionGeneratorSource( const char* name, U&& func )
        {
            using GeneratorSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename GeneratorSourceT::Type;

            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<GeneratorSourceT>::varArray ) / sizeof( *PerDimensionVariable<GeneratorSourceT>::varArray ); idx++ )
            {
                MemberNode member;
                member.name = name;
                member.dimensionIdx = idx;

                member.setFunc = [func, idx]( auto* g, SmartNodeArg<> s )
                {
                    SmartNode<T> downCast = std::dynamic_pointer_cast<T>(s);
                    if( downCast )
                    {
                        g->SetSourceMemberVariable( func( dynamic_cast<GetArg<U, 0>>(g) ).get()[idx], downCast );
                    }
                    return (bool)downCast;
                };

                memberNodes.push_back( member );
            }
        }

        struct MemberHybrid
        {
            const char* name;
            float valueDefault = 0.0f;
            int dimensionIdx = -1;

            std::function<void( Generator*, float )> setValueFunc;
            std::function<bool( Generator*, SmartNodeArg<> )> setNodeFunc;
        };

        template<typename T, typename U>
        void AddHybridSource( const char* name, float defaultValue, void(U::* funcNode)(SmartNodeArg<T>), void(U::* funcValue)(float) )
        {
            MemberHybrid member;
            member.name = name;
            member.valueDefault = defaultValue;

            member.setNodeFunc = [funcNode]( auto* g, SmartNodeArg<> s )
            {
                SmartNode<T> downCast = std::dynamic_pointer_cast<T>(s);
                if( downCast )
                {
                    (dynamic_cast<U*>(g)->*funcNode)( downCast );
                }
                return (bool)downCast;
            };

            member.setValueFunc = [funcValue]( Generator* g, float v )
            {
                (dynamic_cast<U*>(g)->*funcValue)(v);
            };

            memberHybrids.push_back( member );
        }

        template<typename U>
        void AddPerDimensionHybridSource( const char* name, float defaultV, U&& func )
        {
            using HybridSourceT = typename std::invoke_result_t<U, GetArg<U, 0>>::type::Type;
            using T = typename HybridSourceT::Type;

            for( int idx = 0; (size_t)idx < sizeof( PerDimensionVariable<HybridSourceT>::varArray ) / sizeof( *PerDimensionVariable<HybridSourceT>::varArray ); idx++ )
            {
                MemberHybrid member;
                member.name = name;
                member.valueDefault = defaultV;
                member.dimensionIdx = idx;

                member.setNodeFunc = [func, idx]( auto* g, SmartNodeArg<> s )
                {
                    SmartNode<T> downCast = std::dynamic_pointer_cast<T>(s);
                    if( downCast )
                    {
                        g->SetSourceMemberVariable( func( dynamic_cast<GetArg<U, 0>>(g) ).get()[idx], downCast );
                    }
                    return (bool)downCast;
                };

                member.setValueFunc = [func, idx]( Generator* g, float v ) { func( dynamic_cast<GetArg<U, 0>>(g) ).get()[idx] = v; };

                memberHybrids.push_back( member );
            }
        }

        std::uint16_t id;
        const char* name;
        std::vector<const char*> groups;

        std::vector<MemberVariable> memberVariables;
        std::vector<MemberNode>     memberNodes;
        std::vector<MemberHybrid>   memberHybrids;

        virtual Generator* NodeFactory( FastSIMD::eLevel level = FastSIMD::Level_Null ) const = 0;

    private:
        template<typename F, typename Ret, typename... Args>
        static std::tuple<Args...> GetArg_Helper( Ret( F::* )(Args...) const );

        template<typename F, std::size_t I>
        using GetArg = std::tuple_element_t<I, decltype(GetArg_Helper( &F::operator() ))>;

        static std::uint16_t AddMetadataClass( const Metadata* newMetadata )
        {
            sMetadataClasses.emplace_back( newMetadata );

            return (std::uint16_t)sMetadataClasses.size() - 1;
        }

        static std::vector<const Metadata*> sMetadataClasses;
    };

    struct NodeData
    {
        NodeData( const Metadata* metadata );

        const Metadata* metadata;
        std::vector<Metadata::MemberVariable::ValueUnion> variables;
        std::vector<NodeData*> nodes;
        std::vector<std::pair<NodeData*, float>> hybrids;

        bool operator ==( const NodeData& rhs ) const
        {
            return metadata == rhs.metadata &&
                variables == rhs.variables &&
                nodes == rhs.nodes &&
                hybrids == rhs.hybrids;
        }
    };
}

#define FASTNOISE_METADATA( ... ) public:\
    FASTSIMD_LEVEL_SUPPORT( FastNoise::SUPPORTED_SIMD_LEVELS );\
    const FastNoise::Metadata* GetMetadata() const override;\
    struct Metadata : __VA_ARGS__::Metadata{\
    Generator* NodeFactory( FastSIMD::eLevel ) const override;

#define FASTNOISE_METADATA_ABSTRACT( ... ) public:\
    struct Metadata : __VA_ARGS__::Metadata{

