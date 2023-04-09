#include "voxel_memory_pool.h"
#include "../util/macros.h"
#include "../util/memory.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"

namespace zylann::voxel {

namespace {
VoxelMemoryPool *g_memory_pool = nullptr;
} // namespace

void VoxelMemoryPool::create_singleton() {
	ZN_ASSERT(g_memory_pool == nullptr);
	g_memory_pool = ZN_NEW(VoxelMemoryPool);
}

void VoxelMemoryPool::destroy_singleton() {
	const unsigned int used_blocks = VoxelMemoryPool::get_singleton().debug_get_used_blocks();
	if (used_blocks > 0) {
		ZN_PRINT_ERROR(format("VoxelMemoryPool: "
							  "{} memory blocks are still used when unregistering the module. Recycling leak?",
				used_blocks));
#ifdef DEBUG_ENABLED
		VoxelMemoryPool::get_singleton().debug_print_used_blocks(10);
#endif
	}

	ZN_ASSERT(g_memory_pool != nullptr);
	VoxelMemoryPool *pool = g_memory_pool;
	g_memory_pool = nullptr;
	ZN_DELETE(pool);
}

#ifdef DEBUG_ENABLED
void VoxelMemoryPool::debug_print_used_blocks(unsigned int max_count) {
	struct L {
		static void debug_print_used_blocks(const VoxelMemoryPool::DebugUsedBlocks &debug_used_blocks,
				unsigned int &count, unsigned int max_count, size_t mem_size) {
			if (count > max_count) {
				count += debug_used_blocks.blocks.size();
				return;
			}
			const unsigned int initial_count = count;
			for (auto it = debug_used_blocks.blocks.begin(); it != debug_used_blocks.blocks.end(); ++it) {
				if (count > max_count) {
					break;
				}
				std::string s;
				const dstack::Info &info = it->second;
				info.to_string(s);
				if (mem_size == 0) {
					println(format("--- Alloc {}:", count));
				} else {
					println(format("--- Alloc {}, size {}:", count, mem_size));
				}
				println(s);
				++count;
			}
			count = initial_count + debug_used_blocks.blocks.size();
		}
	};

	unsigned int count = 0;
	for (unsigned int pool_index = 0; pool_index < _pot_pools.size(); ++pool_index) {
		const Pool &pool = _pot_pools[pool_index];
		L::debug_print_used_blocks(pool.debug_used_blocks, count, max_count, get_size_from_pool_index(pool_index));
	}
	L::debug_print_used_blocks(_debug_nonpooled_used_blocks, count, max_count, 0);
	if (count > 0 && count > max_count) {
		println(format("[...] and {} more allocs.", max_count - count));
	}
}
#endif

VoxelMemoryPool &VoxelMemoryPool::get_singleton() {
	ZN_ASSERT(g_memory_pool != nullptr);
	return *g_memory_pool;
}

VoxelMemoryPool::VoxelMemoryPool() {}

VoxelMemoryPool::~VoxelMemoryPool() {
#ifdef TOOLS_ENABLED
	if (is_verbose_output_enabled()) {
		debug_print();
	}
#endif
	clear();
}

uint8_t *VoxelMemoryPool::allocate(size_t size) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	// In practice this is not supposed to happen, it might hide a mistake.
	// We should be able to keep running with this only guarantee: the returned value should be able to be "freed"
	// using the same pool.
	ZN_ASSERT_RETURN_V(size != 0, nullptr);

	uint8_t *block = nullptr;
	// Not calculating `pot` immediately because the function we use to calculate it uses 32 bits,
	// while `size_t` can be larger than that.
	if (size > get_highest_supported_size()) {
		// Sorry, memory is not pooled past this size
		block = (uint8_t *)ZN_ALLOC(size * sizeof(uint8_t));
		_total_memory += size;
#ifdef DEBUG_ENABLED
		if (block != nullptr) {
			_debug_nonpooled_used_blocks.add(block);
		}
#endif
	} else {
		const unsigned int pot = get_pool_index_from_size(size);
		Pool &pool = _pot_pools[pot];
		pool.mutex.lock();
		if (pool.blocks.size() > 0) {
			block = pool.blocks.back();
			pool.blocks.pop_back();
			pool.mutex.unlock();
		} else {
			pool.mutex.unlock();
			ZN_PROFILE_SCOPE_NAMED("new alloc");
			// All allocations done in this pool have the same size,
			// which must be greater or equal to `size`
			const size_t capacity = get_size_from_pool_index(pot);
#ifdef DEBUG_ENABLED
			ZN_ASSERT(capacity >= size);
#endif
			block = (uint8_t *)ZN_ALLOC(capacity * sizeof(uint8_t));
			_total_memory += size;
		}
#ifdef DEBUG_ENABLED
		if (block != nullptr) {
			pool.debug_used_blocks.add(block);
		}
#endif
	}
	if (block == nullptr) {
		ZN_PRINT_ERROR("Out of memory");
	} else {
		++_used_blocks;
		_used_memory += size;
	}
	return block;
}

void VoxelMemoryPool::recycle(uint8_t *block, size_t size) {
	// In case we have done empty allocations (we prefer not to do that, but it shouldn't warrant a crash)
	if (block == nullptr && size == 0) {
		return;
	}
	ZN_ASSERT(size != 0);
	ZN_ASSERT(block != nullptr);
	// Not calculating `pot` immediately because the function we use to calculate it uses 32 bits,
	// while `size_t` can be larger than that.
	if (size > get_highest_supported_size()) {
#ifdef DEBUG_ENABLED
		// Make sure this allocation was done by this pool in this scenario
		_debug_nonpooled_used_blocks.remove(block);
#endif
		ZN_FREE(block);
		_total_memory -= size;
	} else {
		const unsigned int pot = get_pool_index_from_size(size);
		Pool &pool = _pot_pools[pot];
#ifdef DEBUG_ENABLED
		// Make sure this allocation was done by this pool in this scenario
		pool.debug_used_blocks.remove(block);
#endif
		MutexLock lock(pool.mutex);
		pool.blocks.push_back(block);
	}
	--_used_blocks;
	_used_memory -= size;
}

void VoxelMemoryPool::clear_unused_blocks() {
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		for (unsigned int i = 0; i < pool.blocks.size(); ++i) {
			void *block = pool.blocks[i];
			ZN_FREE(block);
		}
		_total_memory -= get_size_from_pool_index(pot) * pool.blocks.size();
		pool.blocks.clear();
	}
}

void VoxelMemoryPool::clear() {
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		for (unsigned int i = 0; i < pool.blocks.size(); ++i) {
			void *block = pool.blocks[i];
			ZN_FREE(block);
		}
		pool.blocks.clear();
	}
	_used_memory = 0;
	_total_memory = 0;
	_used_blocks = 0;
}

void VoxelMemoryPool::debug_print() {
	println("-------- VoxelMemoryPool ----------");
	for (unsigned int pot = 0; pot < _pot_pools.size(); ++pot) {
		Pool &pool = _pot_pools[pot];
		MutexLock lock(pool.mutex);
		println(format("Pool {}: {} blocks (capacity {})", pot, pool.blocks.size(), pool.blocks.capacity()));
	}
}

unsigned int VoxelMemoryPool::debug_get_used_blocks() const {
	return _used_blocks;
}

size_t VoxelMemoryPool::debug_get_used_memory() const {
	return _used_memory;
}

size_t VoxelMemoryPool::debug_get_total_memory() const {
	return _total_memory;
}

} // namespace zylann::voxel
