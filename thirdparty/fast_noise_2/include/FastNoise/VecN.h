#pragma once
#include <type_traits>
#include <tuple>
#include <stdexcept>

#include "FastSIMD/FunctionList.h"

template<typename T, size_t Size>
class VecN;

template<typename T>
class VecN<T, 0>
{
protected:
    template<typename... A>
    constexpr VecN( A... ) {}

    template<typename... A>
    void ForEach( A... ) const {}

    template<typename... A>
    void ForEachR( A... ) const {}
};

template<typename T, size_t S>
class VecN : public VecN<T, S - 1>
{
public:
    static constexpr size_t Size = S;

    typedef std::integral_constant<size_t, Size - 1> Index;

    constexpr VecN() : Base(), value() {}

    template<typename... A>
    constexpr VecN( A... args ) :
        Base( args... ),
        value( std::get<Index::value>( std::make_tuple( args... ) ) )
    {
    }

    template<size_t I>
    FS_INLINE std::enable_if_t<(I < Size), T&> At()
    {
        return VecN<T, I + 1>::value;
    }

    template<size_t I>
    FS_INLINE std::enable_if_t<(I < Size), T> At() const
    {
        return VecN<T, I + 1>::value;
    }

    template<size_t I>
    FS_INLINE std::enable_if_t<(I >= Size), T&> At() const
    {
        throw std::out_of_range( "Index of of range" );
    }

    template<typename F, typename... A>
    FS_INLINE void ForEach( F&& func, A&&... other )
    {
        Base::ForEach( func, other... );

        func( Index(), value, (other.template At<Index::value>())... );
    }

    template<typename F, typename... A>
    FS_INLINE void ForEachR( F&& func, A&&... other )
    {
        func( Index(), value, (other.template At<Index::value>())... );

        Base::ForEachR( func, other... );
    }

protected:

    typedef VecN<T, Size - 1> Base;

    typedef std::integral_constant<size_t, Size - 1> Index;

    T value;
};