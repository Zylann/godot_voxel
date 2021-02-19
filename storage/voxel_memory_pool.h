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

	void debug_print();
	unsigned int debug_get_used_blocks() const;

private:
	Pool *get_or_create_pool(uint32_t size);
	void clear();

	HashMap<uint32_t, Pool *> _pools;
	unsigned int _used_blocks = 0;
	Mutex _mutex;
};

#endif // VOXEL_MEMORY_POOL_H
