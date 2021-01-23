#pragma once
#include "FunctionList.h"

template<typename CLASS, typename FS>
class FS_T;

#define FASTSIMD_DECLARE_FS_TYPES \
using float32v = typename FS::float32v;\
using int32v   = typename FS::int32v;\
using mask32v  = typename FS::mask32v
