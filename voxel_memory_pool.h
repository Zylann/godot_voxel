#ifndef VOXEL_MEMORY_POOL_H
#define VOXEL_MEMORY_POOL_H

#include "core/hash_map.h"
#include "core/os/mutex.h"

#include <vector>

// Pool based on a scenario where allocated blocks are often the same size.
// A pool of blocks is assigned for each size.
class VoxelMemoryPool {
private:
	struct Pool {
		std::vector<uint8_t *> blocks;
	};

public:
	static void create_singleton();
	static void destroy_singleton();
	static VoxelMemoryPool *get_singleton();

	VoxelMemoryPool();
	~VoxelMemoryPool();

	uint8_t *allocate(uint32_t size);
	void recycle(uint8_t *block, uint32_t size);
	void clear();

	void debug_print();

private:
	Pool *get_or_create_pool(uint32_t size);

	HashMap<uint32_t, Pool *> _pools;
	Mutex *_mutex = nullptr;
};

#endif // VOXEL_MEMORY_POOL_H
