#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#include <cassert>
#include <type_traits>
#include <functional>

#include "FastNoise_Config.h"

namespace FastNoise
{
    class FASTNOISE_API SmartNodeManager
    {
    public:
        static constexpr uint64_t kInvalidReferenceId = (uint64_t)-1;

        SmartNodeManager() = delete;

        static void SetMemoryPoolSize( uint32_t size );

    private:
        template<typename>
        friend struct MetadataT;

        template<typename>
        friend class SmartNode;

        template<typename T>
        friend SmartNode<T> New( FastSIMD::eLevel );

        static uint64_t GetReference( const void* ptr );

        static void IncReference( uint64_t id );

        static void DecReference( uint64_t id, void* ptr, void ( *destructorFunc )( void* ) );

        static uint32_t ReferenceCount( uint64_t id );

        static void* Allocate( size_t size, size_t align );
    };

    template<typename T>
    class SmartNode
    {
    public:
        static_assert( std::is_base_of<Generator, T>::value, "SmartNode should only be used for FastNoise node classes" );

        template<typename U>
        static SmartNode DynamicCast( SmartNode<U> node )
        {
            if( T* dynamicCast = dynamic_cast<T*>( node.get() ) )
            {
                return FastNoise::SmartNode<T>( node, dynamicCast );
            }

            return nullptr;
        }

        constexpr SmartNode( std::nullptr_t = nullptr ) noexcept :
            mReferenceId( SmartNodeManager::kInvalidReferenceId ),
            mPtr( nullptr )
        {}
        
        SmartNode( const SmartNode& node )
        {
            TryInc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node )
        {
            TryInc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;
        }

        template<typename U>
        SmartNode( const SmartNode<U>& node, T* ptr )
        {
            assert( ptr );

            TryInc( node.mReferenceId );
            mReferenceId = node.mReferenceId;
            mPtr = ptr;
        }

        SmartNode( SmartNode&& node ) noexcept
        {
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;

            node.mReferenceId = SmartNodeManager::kInvalidReferenceId;
            node.mPtr = nullptr;
        }

        template<typename U>
        SmartNode( SmartNode<U>&& node ) noexcept
        {
            mReferenceId = node.mReferenceId;
            mPtr = node.mPtr;

            node.mReferenceId = SmartNodeManager::kInvalidReferenceId;
            node.mPtr = nullptr;
        }

        ~SmartNode()
        {
            Release();
        }

        SmartNode& operator=( SmartNode&& node ) noexcept
        {
            swap( node );
            return *this;
        }

        template<typename U>
        SmartNode& operator=( SmartNode<U>&& node ) noexcept
        {
            if( mReferenceId == node.mReferenceId )
            {
                mPtr = node.mPtr;                
            }
            else
            {
                Release();
                mReferenceId = node.mReferenceId;
                mPtr = node.mPtr;

                node.mReferenceId = SmartNodeManager::kInvalidReferenceId;
                node.mPtr = nullptr;
            }

            return *this;
        }

        SmartNode& operator=( const SmartNode& node ) noexcept
        {
            if( mReferenceId != node.mReferenceId )
            {
                TryInc( node.mReferenceId );
                Release();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

            return *this;
        }

        template<typename U>
        SmartNode& operator=( const SmartNode<U>& node ) noexcept
        {
            if( mReferenceId != node.mReferenceId )
            {
                TryInc( node.mReferenceId );
                Release();
                mReferenceId = node.mReferenceId;
            }
            mPtr = node.mPtr;

            return *this;
        }

        template<typename U>
        friend bool operator==( const SmartNode& lhs, const SmartNode<U>& rhs ) noexcept
        {
            return lhs.get() == rhs.get();
        }

        template<typename U>
        friend bool operator!=( const SmartNode& lhs, const SmartNode<U>& rhs ) noexcept
        {
            return lhs.get() != rhs.get();
        }

        T& operator*() const noexcept
        {
            return *mPtr;
        }

        T* operator->() const noexcept
        {
            return mPtr;
        }

        explicit operator bool() const noexcept
        {
            return mPtr;
        }

        T* get() const noexcept
        {
            return mPtr;
        }

        void reset( T* ptr = nullptr )
        {
            *this = SmartNode( ptr );
        }

        void swap( SmartNode& node ) noexcept
        {
            std::swap( mReferenceId, node.mReferenceId );
            std::swap( mPtr, node.mPtr );
        }

        long use_count() const noexcept
        {
            if( mReferenceId == SmartNodeManager::kInvalidReferenceId )
            {
                return 0;
            }

            return (long)SmartNodeManager::ReferenceCount( mReferenceId );
        }

        bool unique() const noexcept
        {
            return use_count() == 1;
        }

    private:
        template<typename U>
        friend SmartNode<U> New( FastSIMD::eLevel );

        template<typename U>
        friend struct MetadataT;

        template<typename U>
        friend class SmartNode;

        explicit SmartNode( T* ptr ) :
            mReferenceId( ptr ? SmartNodeManager::GetReference( ptr ) : SmartNodeManager::kInvalidReferenceId ),
            mPtr( ptr )
        {
            if( mReferenceId != SmartNodeManager::kInvalidReferenceId )
            {
                SmartNodeManager::IncReference( mReferenceId );
            }
        }

        void Release()
        {
            using U = typename std::remove_const<T>::type;

            if( mReferenceId != SmartNodeManager::kInvalidReferenceId )
            {
                SmartNodeManager::DecReference( mReferenceId, const_cast<U*>( mPtr ), []( void* ptr ) { ( (U*)ptr )->~T(); } );
            }

            mReferenceId = SmartNodeManager::kInvalidReferenceId;
            mPtr = nullptr;            
        }

        static void TryInc( uint64_t id )
        {
            if( id != SmartNodeManager::kInvalidReferenceId )
            {
                SmartNodeManager::IncReference( id );
            }
        }

        uint64_t mReferenceId;
        T* mPtr;
    };
} // namespace FastNoise

namespace std
{
    template<typename T>
    struct hash<FastNoise::SmartNode<T>>
    {
        size_t operator()( const FastNoise::SmartNode<T>& node ) const noexcept
        {
            return std::hash<T*>( node.get() );
        }        
    };
}
