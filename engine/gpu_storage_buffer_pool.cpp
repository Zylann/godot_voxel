#include "gpu_storage_buffer_pool.h"
#include "../util/dstack.h"
#include "../util/godot/core/array.h" // for `varray` in GDExtension builds
#include "../util/godot/core/print_string.h"
#include "../util/godot/core/string.h" // for `+=` operator missing from String in GDExtension builds
#include "../util/math/funcs.h"
#include "../util/profiling.h"
#include "../util/string_funcs.h"

#include <algorithm>
#include <limits>

namespace zylann::voxel {

GPUStorageBufferPool::GPUStorageBufferPool() {
	uint32_t s = 1;
	for (unsigned int i = 0; i < _pool_sizes.size(); ++i) {
		// Have sizes aligned to 4 bytes
		_pool_sizes[i] = s * 4;
		s = math::max(s + 1, s + s / 2);
	}
}

GPUStorageBufferPool::~GPUStorageBufferPool() {
	if (_rendering_device != nullptr) {
		clear();
	} else {
		for (unsigned int i = 0; i < _pool_sizes.size(); ++i) {
			ZN_ASSERT_CONTINUE_MSG(_pools[i].buffers.size() == 0, "Possibly leaked buffers?");
		}
	}
}

void GPUStorageBufferPool::clear() {
	ZN_ASSERT_RETURN(_rendering_device != nullptr);
	ZN_DSTACK();
	RenderingDevice &rd = *_rendering_device;
	unsigned int pool_index = 0;
	for (Pool &pool : _pools) {
		if (pool.used_buffers > 0) {
			ZN_PRINT_ERROR(
					format("{} storage buffers are still in use when clearing pool {}", pool.used_buffers, pool_index));
		}
		for (GPUStorageBuffer &b : pool.buffers) {
			free_rendering_device_rid(rd, b.rid);
		}
		pool.buffers.clear();
		++pool_index;
	}
}

void GPUStorageBufferPool::set_rendering_device(RenderingDevice *rd) {
	if (_rendering_device != nullptr) {
		// Clear data from the previous device
		clear();
	} else {
		for (unsigned int i = 0; i < _pool_sizes.size(); ++i) {
			ZN_ASSERT_CONTINUE_MSG(_pools[i].buffers.size() == 0, "Possibly leaked buffers?");
		}
	}
	_rendering_device = rd;
}

unsigned int GPUStorageBufferPool::get_pool_index_from_size(uint32_t p_size) const {
	// Find first size that is equal or higher than the given size.
	// (first size that does not satisfy `pool_size < p_size`)
	auto it = std::lower_bound(_pool_sizes.begin(), _pool_sizes.end(), p_size);
#ifdef DEBUG_ENABLED
	if (it != _pool_sizes.end()) {
		ZN_ASSERT(*it >= p_size);
	}
#endif
	return it - _pool_sizes.begin();
}

GPUStorageBuffer GPUStorageBufferPool::allocate(const PackedByteArray &pba, unsigned int post_barrier) {
	return allocate(pba.size(), &pba, post_barrier);
}

GPUStorageBuffer GPUStorageBufferPool::allocate(uint32_t p_size, unsigned int post_barrier) {
	return allocate(p_size, nullptr, post_barrier);
}

GPUStorageBuffer GPUStorageBufferPool::allocate(
		uint32_t p_size, const PackedByteArray *pba, unsigned int post_barrier) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN_V(p_size > 0, GPUStorageBuffer());

	ZN_ASSERT_RETURN_V(_rendering_device != nullptr, GPUStorageBuffer());
	RenderingDevice &rd = *_rendering_device;

	const unsigned int pool_index = get_pool_index_from_size(p_size);
	ZN_ASSERT_RETURN_V(pool_index < _pools.size(), GPUStorageBuffer());
	Pool &pool = _pools[pool_index];

	GPUStorageBuffer b;

	if (pool.buffers.size() == 0) {
		const unsigned int capacity = _pool_sizes[pool_index];
		// Unfortunately `storage_buffer_create` in the Godot API requires that you provide a PoolByteArray of the exact
		// same size, when provided. Our pooling strategy means we are often allocating a bit more than initially
		// requested. The passed data would fit, but Godot doesn't want that... so in order to avoid having to create an
		// extended copy of the PackedByteArray, we don't initialize the buffer on creation. Instead, we do it with a
		// separate call... I have no idea if that has a particular performance impact, apart from more RID lookups.
		// b.rid = rd.storage_buffer_create(capacity, pba);
		b.rid = rd.storage_buffer_create(capacity);
		ZN_ASSERT_RETURN_V(b.rid.is_valid(), GPUStorageBuffer());
		if (pba != nullptr) {
			update_storage_buffer(rd, b.rid, 0, pba->size(), *pba, post_barrier);
		}
		b.size = capacity;

	} else {
		b = pool.buffers.back();
		ZN_ASSERT(b.is_valid());
		pool.buffers.pop_back();
		if (pba != nullptr) {
			update_storage_buffer(rd, b.rid, 0, p_size, *pba, post_barrier);
		}
	}

	++pool.used_buffers;

	return b;
}

void GPUStorageBufferPool::recycle(GPUStorageBuffer b) {
	ZN_ASSERT_RETURN(b.rid.is_valid());
	ZN_ASSERT_RETURN(b.size != 0);

	ZN_ASSERT_RETURN(_rendering_device != nullptr);

	const unsigned int pool_index = get_pool_index_from_size(b.size);
	ZN_ASSERT_RETURN(pool_index < _pools.size());
	Pool &pool = _pools[pool_index];

	ZN_ASSERT(pool.used_buffers > 0);
	--pool.used_buffers;

#if DEV_ENABLED
	for (const GPUStorageBuffer &sb : pool.buffers) {
		ZN_ASSERT_MSG(sb.rid != b.rid, "Pooling twice the same storage buffer");
	}
#endif

	pool.buffers.push_back(b);
}

void GPUStorageBufferPool::debug_print() const {
	String s = "---- GPUStorageBufferPool ----\n";
	for (unsigned int i = 0; i < _pools.size(); ++i) {
		const Pool &pool = _pools[i];
		if (pool.buffers.capacity() == 0) {
			continue;
		}
		const unsigned int block_size = _pool_sizes[i];
		s += String("Pool[{0}] block size: {1}, pooled buffers: {2}, capacity: {3}\n")
					 .format(varray(i, block_size, ZN_SIZE_T_TO_VARIANT(pool.buffers.size()),
							 ZN_SIZE_T_TO_VARIANT(pool.buffers.capacity())));
	}
	s += "----";
	print_line(s);
}

} // namespace zylann::voxel
