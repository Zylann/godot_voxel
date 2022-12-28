#ifndef VOXEL_GPU_STORAGE_BUFFER_H
#define VOXEL_GPU_STORAGE_BUFFER_H

#include "../util/fixed_array.h"
#include "../util/godot/classes/rendering_device.h"
#include <array>
#include <vector>

namespace zylann::voxel {

struct GPUStorageBuffer {
	RID rid;
	size_t size = 0;

	inline bool is_null() const {
		// Can't use `is_null()`, core has it but GDExtension doesn't have it
		return !rid.is_valid();
	}

	inline bool is_valid() const {
		return rid.is_valid();
	}
};

// Pools storage buffers of specific sizes so they can be re-used.
// Not thread-safe.
class GPUStorageBufferPool {
public:
	GPUStorageBufferPool();
	~GPUStorageBufferPool();

	void clear();
	void set_rendering_device(RenderingDevice *rd);
	GPUStorageBuffer allocate(
			const PackedByteArray &pba, unsigned int post_barrier = RenderingDevice::BARRIER_MASK_ALL_BARRIERS);
	GPUStorageBuffer allocate(uint32_t p_size, unsigned int post_barrier = RenderingDevice::BARRIER_MASK_ALL_BARRIERS);
	void recycle(GPUStorageBuffer b);
	void debug_print() const;

private:
	GPUStorageBuffer allocate(uint32_t p_size, const PackedByteArray *pba, unsigned int post_barrier);

	unsigned int get_pool_index_from_size(uint32_t p_size) const;

	struct Pool {
		std::vector<GPUStorageBuffer> buffers;
		unsigned int used_buffers = 0;
	};

	// Up to roughly 800 Mb with the current size formula
	static const unsigned int POOL_COUNT = 48;

	std::array<uint32_t, POOL_COUNT> _pool_sizes;
	FixedArray<Pool, POOL_COUNT> _pools;
	RenderingDevice *_rendering_device = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GPU_STORAGE_BUFFER_H
