#pragma once

#include <cinttypes>

#include "FastSIMD/FastSIMD.h"
#include "FastSIMD/FunctionList.h"

#define FASTSIMD_INTERNAL_TYPE_SET( CLASS, TYPE )                           \
TYPE vector;									                            \
FS_INLINE CLASS() { }                                                       \
FS_INLINE CLASS( const TYPE& v ) : vector(v) {};	                        \
FS_INLINE CLASS& operator = ( const TYPE& v ) { vector = v; return *this; } \
FS_INLINE operator TYPE() const { return vector; }

#define FASTSIMD_INTERNAL_OPERATOR( TYPE, TYPE2, OPERATOR, OPERATOREQUALS )	\
FS_INLINE static TYPE operator OPERATOR ( TYPE lhs, TYPE2 rhs )             \
{											                                \
    lhs OPERATOREQUALS rhs;								                    \
    return lhs;								                                \
}

#define FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE, TYPE2, OPERATOR, OPERATOREQUALS ) \
template<FastSIMD::eLevel L>                                                          \
FS_INLINE static TYPE operator OPERATOR ( TYPE lhs, TYPE2 rhs )                       \
{											                                          \
    lhs OPERATOREQUALS rhs;								                              \
    return lhs;								                                          \
}

#define FASTSIMD_INTERNAL_OPERATORS_FLOAT( TYPE )      \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, +, += ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, -, -= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, *, *= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, /, /= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, &, &= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, |, |= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, ^, ^= ) 

#define FASTSIMD_INTERNAL_OPERATORS_FLOAT_TEMPLATED( TYPE )            \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, +, += ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, -, -= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, *, *= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, /, /= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, &, &= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, |, |= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, ^, ^= ) 

#define FASTSIMD_INTERNAL_OPERATORS_INT( TYPE, TYPE2 ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, +, += ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, -, -= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, *, *= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, &, &= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, |, |= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, const TYPE&, ^, ^= ) \
FASTSIMD_INTERNAL_OPERATOR( TYPE, TYPE2, >>, >>= )     \
FASTSIMD_INTERNAL_OPERATOR( TYPE, TYPE2, <<, <<= )

#define FASTSIMD_INTERNAL_OPERATORS_INT_TEMPLATED( TYPE, TYPE2 )       \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, +, += ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, -, -= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, *, *= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, &, &= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, |, |= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, const TYPE<L>&, ^, ^= ) \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, TYPE2, >>, >>= )        \
FASTSIMD_INTERNAL_OPERATOR_TEMPLATED( TYPE<L>, TYPE2, <<, <<= )
