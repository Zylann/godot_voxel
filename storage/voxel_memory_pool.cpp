#include "voxel_memory_pool.h"
#include "../util/macros.h"
#include "../util/profiling.h"

#include <core/os/os.h>
#include <core/print_string.h>
#include <core/variant.h>

namespace {
VoxelMemoryPool *g_memory_pool = nullptr;
} // namespace

void VoxelMemoryPool::create_singleton() {
	CRASH_COND(g_memory_pool != nullptr);
	g_memory_pool = memnew(VoxelMemoryPool);
}

void VoxelMemoryPool::destroy_singleton() {
	CRASH_COND(g_memory_pool == nullptr);
	VoxelMemoryPool *pool = g_memory_pool;
	g_memory_pool = nullptr;
	memdelete(pool);
}

VoxelMemoryPool *VoxelMemoryPool::get_singleton() {
	CRASH_COND(g_memory_pool == nullptr);
	return g_memory_pool;
}

VoxelMemoryPool::VoxelMemoryPool() {
}

VoxelMemoryPool::~VoxelMemoryPool() {
#ifdef TOOLS_ENABLED
	if (OS::get_singleton()->is_stdout_verbose()) {
		debug_print();
	}
#endif
	clear();
}

uint8_t *VoxelMemoryPool::allocate(size_t size) {
	VOXEL_PROFILE_SCOPE();
	MutexLock lock(_mutex);
	Pool *pool = get_or_create_pool(size);
	uint8_t *block;
	if (pool->blocks.size() > 0) {
		block = pool->blocks.back();
		pool->blocks.pop_back();
	} else {
		block = (uint8_t *)memalloc(size * sizeof(uint8_t));
		ERR_FAIL_COND_V(block == nullptr, nullptr);
		_total_memory += size;
	}
	++_used_blocks;
	_used_memory += size;
	return block;
}

void VoxelMemoryPool::recycle(uint8_t *block, size_t size) {
	CRASH_COND(size == 0);
	CRASH_COND(block == nullptr);
	MutexLock lock(_mutex);
	Pool *pool = _pools[size]; // If not found, entry will be created! It would be an error
	// Check recycling before having allocated
	CRASH_COND(pool == nullptr);
	pool->blocks.push_back(block);
	--_used_blocks;
	_used_memory -= size;
}

void VoxelMemoryPool::clear_unused_blocks() {
	MutexLock lock(_mutex);
	const size_t *key = nullptr;
	while ((key = _pools.next(key))) {
		Pool *pool = _pools.get(*key);
		CRASH_COND(pool == nullptr);
		for (auto it = pool->blocks.begin(); it != pool->blocks.end(); ++it) {
			uint8_t *ptr = *it;
			CRASH_COND(ptr == nullptr);
			memfree(ptr);
		}
		_total_memory -= (*key) * pool->blocks.size();
		pool->blocks.clear();
	}
}

void VoxelMemoryPool::clear() {
	MutexLock lock(_mutex);
	const size_t *key = nullptr;
	while ((key = _pools.next(key))) {
		Pool *pool = _pools.get(*key);
		CRASH_COND(pool == nullptr);
		for (auto it = pool->blocks.begin(); it != pool->blocks.end(); ++it) {
			uint8_t *ptr = *it;
			CRASH_COND(ptr == nullptr);
			memfree(ptr);
		}
		memdelete(pool);
	}
	_pools.clear();
	_used_memory = 0;
	_total_memory = 0;
	_used_blocks = 0;
}

void VoxelMemoryPool::debug_print() {
	MutexLock lock(_mutex);
	print_line("-------- VoxelMemoryPool ----------");
	if (_pools.size() == 0) {
		print_line("No pools created");
	} else {
		const size_t *key = nullptr;
		int i = 0;
		while ((key = _pools.next(key))) {
			Pool *pool = _pools.get(*key);
			print_line(String("Pool {0} for size {1}: {2} blocks")
							   .format(varray(i, SIZE_T_TO_VARIANT(*key), SIZE_T_TO_VARIANT(pool->blocks.size()))));
			++i;
		}
	}
}

unsigned int VoxelMemoryPool::debug_get_used_blocks() const {
	//MutexLock lock(_mutex);
	return _used_blocks;
}

size_t VoxelMemoryPool::debug_get_used_memory() const {
	//MutexLock lock(_mutex);
	return _used_memory;
}

size_t VoxelMemoryPool::debug_get_total_memory() const {
	//MutexLock lock(_mutex);
	return _total_memory;
}

VoxelMemoryPool::Pool *VoxelMemoryPool::get_or_create_pool(size_t size) {
	Pool *pool;
	Pool **ppool = _pools.getptr(size);
	if (ppool == nullptr) {
		pool = memnew(Pool);
		CRASH_COND(pool == nullptr);
		_pools.set(size, pool);
	} else {
		pool = *ppool;
		CRASH_COND(pool == nullptr);
	}
	return pool;
}
