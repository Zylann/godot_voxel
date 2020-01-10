#include "voxel_memory_pool.h"
#include "core/print_string.h"
#include "core/variant.h"

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
	_mutex = Mutex::create();
}

VoxelMemoryPool::~VoxelMemoryPool() {
	debug_print();
	clear();
	memdelete(_mutex);
}

uint8_t *VoxelMemoryPool::allocate(uint32_t size) {
	MutexLock lock(_mutex);
	Pool *pool = get_or_create_pool(size);
	uint8_t *block;
	if (pool->blocks.size() > 0) {
		block = pool->blocks.back();
		pool->blocks.pop_back();
	} else {
		block = (uint8_t *)memalloc(size * sizeof(uint8_t));
	}
	return block;
}

void VoxelMemoryPool::recycle(uint8_t *block, uint32_t size) {
	MutexLock lock(_mutex);
	Pool *pool = _pools[size]; // If not found, entry will be created! It would be an error
	// Check recycling before having allocated
	CRASH_COND(pool == nullptr);
	pool->blocks.push_back(block);
}

void VoxelMemoryPool::clear() {
	MutexLock lock(_mutex);
	const uint32_t *key = NULL;
	while ((key = _pools.next(key))) {
		Pool *pool = _pools.get(*key);
		CRASH_COND(pool == nullptr);
		for (auto it = pool->blocks.begin(); it != pool->blocks.end(); ++it) {
			uint8_t *ptr = *it;
			CRASH_COND(ptr == nullptr);
			memfree(ptr);
		}
	}
	_pools.clear();
}

void VoxelMemoryPool::debug_print() {
	MutexLock lock(_mutex);
	print_line("-------- VoxelMemoryPool ----------");
	const uint32_t *key = NULL;
	int i = 0;
	while ((key = _pools.next(key))) {
		Pool *pool = _pools.get(*key);
		print_line(String("Pool {0} for size {1}: {2} blocks").format(varray(i, *key, pool->blocks.size())));
		++i;
	}
}

VoxelMemoryPool::Pool *VoxelMemoryPool::get_or_create_pool(uint32_t size) {
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
