#include "voxel_map.h"
#include "../cube_tables.h"
#include "../util/macros.h"
#include "voxel_block.h"

VoxelMap::VoxelMap() :
		_last_accessed_block(nullptr) {

	// TODO Make it configurable in editor (with all necessary notifications and updatings!)
	set_block_size_pow2(4);

	_default_voxel.fill(0);
	_default_voxel[VoxelBuffer::CHANNEL_SDF] = 255;
}

VoxelMap::~VoxelMap() {
	PRINT_VERBOSE("Destroying VoxelMap");
	clear();
}

void VoxelMap::create(unsigned int block_size_po2, int lod_index) {
	clear();
	set_block_size_pow2(block_size_po2);
	set_lod_index(lod_index);
}

void VoxelMap::set_block_size_pow2(unsigned int p) {
	ERR_FAIL_COND_MSG(p < 1, "Block size is too small");
	ERR_FAIL_COND_MSG(p > 8, "Block size is too big");

	_block_size_pow2 = p;
	_block_size = 1 << _block_size_pow2;
	_block_size_mask = _block_size - 1;
}

void VoxelMap::set_lod_index(int lod_index) {
	ERR_FAIL_COND_MSG(lod_index < 0, "LOD index can't be negative");
	ERR_FAIL_COND_MSG(lod_index >= 32, "LOD index is too big");

	_lod_index = lod_index;
}

unsigned int VoxelMap::get_lod_index() const {
	return _lod_index;
}

int VoxelMap::get_voxel(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelBlock *block = get_block(bpos);
	if (block == nullptr) {
		return _default_voxel[c];
	}
	RWLockRead lock(block->voxels->get_lock());
	return block->voxels->get_voxel(to_local(pos), c);
}

VoxelBlock *VoxelMap::get_or_create_block_at_voxel_pos(Vector3i pos) {
	Vector3i bpos = voxel_to_block(pos);
	VoxelBlock *block = get_block(bpos);

	if (block == nullptr) {
		Ref<VoxelBuffer> buffer(memnew(VoxelBuffer));
		buffer->create(_block_size, _block_size, _block_size);
		buffer->set_default_values(_default_voxel);

		block = VoxelBlock::create(bpos, buffer, _block_size, _lod_index);

		set_block(bpos, block);
	}

	return block;
}

void VoxelMap::set_voxel(int value, Vector3i pos, unsigned int c) {
	VoxelBlock *block = get_or_create_block_at_voxel_pos(pos);
	// TODO If it turns out to be a problem, use CoW
	RWLockWrite lock(block->voxels->get_lock());
	block->voxels->set_voxel(value, to_local(pos), c);
}

float VoxelMap::get_voxel_f(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelBlock *block = get_block(bpos);
	if (block == nullptr) {
		return _default_voxel[c];
	}
	Vector3i lpos = to_local(pos);
	RWLockRead lock(block->voxels->get_lock());
	return block->voxels->get_voxel_f(lpos.x, lpos.y, lpos.z, c);
}

void VoxelMap::set_voxel_f(real_t value, Vector3i pos, unsigned int c) {
	VoxelBlock *block = get_or_create_block_at_voxel_pos(pos);
	Vector3i lpos = to_local(pos);
	RWLockWrite lock(block->voxels->get_lock());
	block->voxels->set_voxel_f(value, lpos.x, lpos.y, lpos.z, c);
}

void VoxelMap::set_default_voxel(int value, unsigned int channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_default_voxel[channel] = value;
}

int VoxelMap::get_default_voxel(unsigned int channel) {
	ERR_FAIL_INDEX_V(channel, VoxelBuffer::MAX_CHANNELS, 0);
	return _default_voxel[channel];
}

VoxelBlock *VoxelMap::get_block(Vector3i bpos) {
	if (_last_accessed_block && _last_accessed_block->position == bpos) {
		return _last_accessed_block;
	}
	unsigned int *iptr = _blocks_map.getptr(bpos);
	if (iptr != nullptr) {
		const unsigned int i = *iptr;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		VoxelBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		_last_accessed_block = block;
		return _last_accessed_block;
	}
	return nullptr;
}

const VoxelBlock *VoxelMap::get_block(Vector3i bpos) const {
	if (_last_accessed_block && _last_accessed_block->position == bpos) {
		return _last_accessed_block;
	}
	const unsigned int *iptr = _blocks_map.getptr(bpos);
	if (iptr != nullptr) {
		const unsigned int i = *iptr;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		// TODO This function can't cache _last_accessed_block, because it's const, so repeated accesses are hashing again...
		const VoxelBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		return block;
	}
	return nullptr;
}

void VoxelMap::set_block(Vector3i bpos, VoxelBlock *block) {
	ERR_FAIL_COND(block == nullptr);
	CRASH_COND(bpos != block->position);
	if (_last_accessed_block == nullptr || _last_accessed_block->position == bpos) {
		_last_accessed_block = block;
	}
#ifdef DEBUG_ENABLED
	CRASH_COND(_blocks_map.has(bpos));
#endif
	unsigned int i = _blocks.size();
	_blocks.push_back(block);
	_blocks_map.set(bpos, i);
}

void VoxelMap::remove_block_internal(Vector3i bpos, unsigned int index) {
	// This function assumes the block is already freed
	_blocks_map.erase(bpos);

	VoxelBlock *moved_block = _blocks.back();
	_blocks[index] = moved_block;
	_blocks.pop_back();

	if (index < _blocks.size()) {
		unsigned int *moved_block_index = _blocks_map.getptr(moved_block->position);
		CRASH_COND(moved_block_index == nullptr);
		*moved_block_index = index;
	}
}

VoxelBlock *VoxelMap::set_block_buffer(Vector3i bpos, Ref<VoxelBuffer> buffer) {
	ERR_FAIL_COND_V(buffer.is_null(), nullptr);
	VoxelBlock *block = get_block(bpos);
	if (block == nullptr) {
		block = VoxelBlock::create(bpos, *buffer, _block_size, _lod_index);
		set_block(bpos, block);
	} else {
		block->voxels = buffer;
	}
	return block;
}

bool VoxelMap::has_block(Vector3i pos) const {
	return /*(_last_accessed_block != nullptr && _last_accessed_block->pos == pos) ||*/ _blocks_map.has(pos);
}

bool VoxelMap::is_block_surrounded(Vector3i pos) const {
	// TODO If that check proves to be too expensive with all blocks we deal with, cache it in VoxelBlocks
	for (unsigned int i = 0; i < Cube::MOORE_NEIGHBORING_3D_COUNT; ++i) {
		Vector3i bpos = pos + Cube::g_moore_neighboring_3d[i];
		if (!has_block(bpos)) {
			return false;
		}
	}
	return true;
}

void VoxelMap::get_buffer_copy(Vector3i min_pos, VoxelBuffer &dst_buffer, unsigned int channels_mask) {
	const Vector3i max_pos = min_pos + dst_buffer.get_size();

	const Vector3i min_block_pos = voxel_to_block(min_pos);
	const Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1, 1, 1)) + Vector3i(1, 1, 1);
	// TODO Why is this function limited by this check?
	// Probably to make sure we are getting neighbors, however that's a worry for the caller, not this function...
	ERR_FAIL_COND((max_block_pos - min_block_pos) != Vector3i(3, 3, 3));

	const Vector3i block_size_v(_block_size, _block_size, _block_size);

	for (unsigned int channel = 0; channel < VoxelBuffer::MAX_CHANNELS; ++channel) {
		if (((1 << channel) & channels_mask) == 0) {
			continue;
		}

		Vector3i bpos;
		for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
			for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
				for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
					const VoxelBlock *block = get_block(bpos);

					if (block) {
						const VoxelBuffer &src_buffer = **block->voxels;

						dst_buffer.set_channel_depth(channel, src_buffer.get_channel_depth(channel));

						const Vector3i offset = block_to_voxel(bpos);

						RWLockRead lock(src_buffer.get_lock());

						// Note: copy_from takes care of clamping the area if it's on an edge
						dst_buffer.copy_from(src_buffer,
								min_pos - offset,
								max_pos - offset,
								offset - min_pos,
								channel);

					} else {
						// For now, inexistent blocks default to hardcoded defaults, corresponding to "empty space".
						// If we want to change this, we may have to add an API for that.
						const Vector3i offset = block_to_voxel(bpos);
						dst_buffer.fill_area(
								_default_voxel[channel],
								offset - min_pos,
								offset - min_pos + block_size_v,
								channel);
					}
				}
			}
		}
	}
}

void VoxelMap::clear() {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		VoxelBlock *block = *it;
		if (block == nullptr) {
			ERR_PRINT("Unexpected nullptr in VoxelMap::clear()");
		} else {
			memdelete(block);
		}
	}
	_blocks.clear();
	_blocks_map.clear();
	_last_accessed_block = nullptr;
}

int VoxelMap::get_block_count() const {
#ifdef DEBUG_ENABLED
	const unsigned int blocks_map_size = _blocks_map.size();
	CRASH_COND(_blocks.size() != blocks_map_size);
#endif
	return _blocks.size();
}

bool VoxelMap::is_area_fully_loaded(const Rect3i voxels_box) const {
	Rect3i block_box = voxels_box.downscaled(get_block_size());
	return block_box.all_cells_match([this](Vector3i pos) {
		return has_block(pos);
	});
}
