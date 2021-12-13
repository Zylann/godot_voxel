// This file is part of meshoptimizer library; see meshoptimizer.h for version/license details
#include "meshoptimizer.h"

MESHOPTIMIZER_ZYLANN_NAMESPACE_BEGIN

void meshopt_setAllocator(void* (*allocate)(size_t), void (*deallocate)(void*))
{
	meshopt_Allocator::Storage::allocate = allocate;
	meshopt_Allocator::Storage::deallocate = deallocate;
}

MESHOPTIMIZER_ZYLANN_NAMESPACE_END

