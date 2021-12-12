#include "voxel_data_map.h"
#include "../constants/cube_tables.h"
#include "../generators/voxel_generator.h"
#include "../streams/voxel_block_request.h"
#include "../util/godot/funcs.h"
#include "../util/macros.h"

#include <limits>

VoxelDataMap::VoxelDataMap() {
	// TODO Make it configurable in editor (with all necessary notifications and updatings!)
	set_block_size_pow2(VoxelConstants::DEFAULT_BLOCK_SIZE_PO2);

	// Pick same defaults as VoxelBuffer
	VoxelBufferInternal b;
	b.create(1, 1, 1);
	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		_default_voxel[channel_index] = b.get_voxel(0, 0, 0, channel_index);
	}
}

VoxelDataMap::~VoxelDataMap() {
	clear();
}

void VoxelDataMap::create(unsigned int block_size_po2, int lod_index) {
	clear();
	set_block_size_pow2(block_size_po2);
	set_lod_index(lod_index);
}

void VoxelDataMap::set_block_size_pow2(unsigned int p) {
	ERR_FAIL_COND_MSG(p < 1, "Block size is too small");
	ERR_FAIL_COND_MSG(p > 8, "Block size is too big");

	_block_size_pow2 = p;
	_block_size = 1 << _block_size_pow2;
	_block_size_mask = _block_size - 1;
}

void VoxelDataMap::set_lod_index(int lod_index) {
	ERR_FAIL_COND_MSG(lod_index < 0, "LOD index can't be negative");
	ERR_FAIL_COND_MSG(lod_index >= 32, "LOD index is too big");

	_lod_index = lod_index;
}

unsigned int VoxelDataMap::get_lod_index() const {
	return _lod_index;
}

int VoxelDataMap::get_voxel(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelDataBlock *block = get_block(bpos);
	if (block == nullptr) {
		return _default_voxel[c];
	}
	RWLockRead lock(block->get_voxels_const().get_lock());
	return block->get_voxels_const().get_voxel(to_local(pos), c);
}

VoxelDataBlock *VoxelDataMap::create_default_block(Vector3i bpos) {
	std::shared_ptr<VoxelBufferInternal> buffer = gd_make_shared<VoxelBufferInternal>();
	buffer->create(_block_size, _block_size, _block_size);
	buffer->set_default_values(_default_voxel);
	VoxelDataBlock *block = VoxelDataBlock::create(bpos, buffer, _block_size, _lod_index);
	set_block(bpos, block);
	return block;
}

VoxelDataBlock *VoxelDataMap::get_or_create_block_at_voxel_pos(Vector3i pos) {
	Vector3i bpos = voxel_to_block(pos);
	VoxelDataBlock *block = get_block(bpos);
	if (block == nullptr) {
		block = create_default_block(bpos);
	}
	return block;
}

void VoxelDataMap::set_voxel(int value, Vector3i pos, unsigned int c) {
	VoxelDataBlock *block = get_or_create_block_at_voxel_pos(pos);
	// TODO If it turns out to be a problem, use CoW
	VoxelBufferInternal &voxels = block->get_voxels();
	RWLockWrite lock(voxels.get_lock());
	voxels.set_voxel(value, to_local(pos), c);
}

float VoxelDataMap::get_voxel_f(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelDataBlock *block = get_block(bpos);
	if (block == nullptr) {
		// TODO Not valid for a float return value
		return _default_voxel[c];
	}
	Vector3i lpos = to_local(pos);
	RWLockRead lock(block->get_voxels_const().get_lock());
	return block->get_voxels_const().get_voxel_f(lpos.x, lpos.y, lpos.z, c);
}

void VoxelDataMap::set_voxel_f(real_t value, Vector3i pos, unsigned int c) {
	VoxelDataBlock *block = get_or_create_block_at_voxel_pos(pos);
	Vector3i lpos = to_local(pos);
	VoxelBufferInternal &voxels = block->get_voxels();
	RWLockWrite lock(voxels.get_lock());
	voxels.set_voxel_f(value, lpos.x, lpos.y, lpos.z, c);
}

void VoxelDataMap::set_default_voxel(int value, unsigned int channel) {
	ERR_FAIL_INDEX(channel, VoxelBufferInternal::MAX_CHANNELS);
	_default_voxel[channel] = value;
}

int VoxelDataMap::get_default_voxel(unsigned int channel) {
	ERR_FAIL_INDEX_V(channel, VoxelBufferInternal::MAX_CHANNELS, 0);
	return _default_voxel[channel];
}

VoxelDataBlock *VoxelDataMap::get_block(Vector3i bpos) {
	auto it = _blocks_map.find(bpos);
	if (it != _blocks_map.end()) {
		const unsigned int i = it->second;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		VoxelDataBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		return block;
	}
	return nullptr;
}

const VoxelDataBlock *VoxelDataMap::get_block(Vector3i bpos) const {
	auto it = _blocks_map.find(bpos);
	if (it != _blocks_map.end()) {
		const unsigned int i = it->second;
#ifdef DEBUG_ENABLED
		CRASH_COND(i >= _blocks.size());
#endif
		// TODO This function can't cache _last_accessed_block, because it's const, so repeated accesses are hashing again...
		const VoxelDataBlock *block = _blocks[i];
		CRASH_COND(block == nullptr); // The map should not contain null blocks
		return block;
	}
	return nullptr;
}

void VoxelDataMap::set_block(Vector3i bpos, VoxelDataBlock *block) {
	ERR_FAIL_COND(block == nullptr);
	CRASH_COND(bpos != block->position);
#ifdef DEBUG_ENABLED
	CRASH_COND(_blocks_map.find(bpos) != _blocks_map.end());
#endif
	unsigned int i = _blocks.size();
	_blocks.push_back(block);
	_blocks_map.insert(std::make_pair(bpos, i));
}

void VoxelDataMap::remove_block_internal(Vector3i bpos, unsigned int index) {
	// TODO `erase` can occasionally be very slow (milliseconds) if the map contains lots of items.
	// This might be caused by internal rehashing/resizing.
	// We should look for a faster container, or reduce the number of entries.

	// This function assumes the block is already freed
	_blocks_map.erase(bpos);

	VoxelDataBlock *moved_block = _blocks.back();
#ifdef DEBUG_ENABLED
	CRASH_COND(index >= _blocks.size());
#endif
	_blocks[index] = moved_block;
	_blocks.pop_back();

	if (index < _blocks.size()) {
		auto it = _blocks_map.find(moved_block->position);
		CRASH_COND(it == _blocks_map.end());
		it->second = index;
	}
}

VoxelDataBlock *VoxelDataMap::set_block_buffer(Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer,
		bool overwrite) {
	ERR_FAIL_COND_V(buffer == nullptr, nullptr);
	VoxelDataBlock *block = get_block(bpos);
	if (block == nullptr) {
		block = VoxelDataBlock::create(bpos, buffer, _block_size, _lod_index);
		set_block(bpos, block);
	} else if (overwrite) {
		block->set_voxels(buffer);
	} else {
		PRINT_VERBOSE(String("Discarded block {0} lod {1}, there was already data and overwriting is not enabled")
							  .format(varray(bpos.to_vec3(), _lod_index)));
	}
	return block;
}

bool VoxelDataMap::has_block(Vector3i pos) const {
	return _blocks_map.find(pos) != _blocks_map.end();
}

bool VoxelDataMap::is_block_surrounded(Vector3i pos) const {
	// TODO If that check proves to be too expensive with all blocks we deal with, cache it in VoxelBlocks
	for (unsigned int i = 0; i < Cube::MOORE_NEIGHBORING_3D_COUNT; ++i) {
		Vector3i bpos = pos + Cube::g_moore_neighboring_3d[i];
		if (!has_block(bpos)) {
			return false;
		}
	}
	return true;
}

void VoxelDataMap::copy(Vector3i min_pos, VoxelBufferInternal &dst_buffer, unsigned int channels_mask,
		void *callback_data, void (*gen_func)(void *, VoxelBufferInternal &, Vector3i)) const {
	const Vector3i max_pos = min_pos + dst_buffer.get_size();

	const Vector3i min_block_pos = voxel_to_block(min_pos);
	const Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1, 1, 1)) + Vector3i(1, 1, 1);

	const Vector3i block_size_v(_block_size, _block_size, _block_size);

	unsigned int channels_count;
	FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels =
			VoxelBufferInternal::mask_to_channels_list(channels_mask, channels_count);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				const VoxelDataBlock *block = get_block(bpos);
				const Vector3i src_block_origin = block_to_voxel(bpos);

				if (block != nullptr) {
					const VoxelBufferInternal &src_buffer = block->get_voxels_const();

					RWLockRead rlock(src_buffer.get_lock());

					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						const uint8_t channel = channels[ci];
						dst_buffer.set_channel_depth(channel, src_buffer.get_channel_depth(channel));
						// Note: copy_from takes care of clamping the area if it's on an edge
						dst_buffer.copy_from(src_buffer,
								min_pos - src_block_origin,
								src_buffer.get_size(),
								Vector3i(),
								channel);
					}

				} else if (gen_func != nullptr) {
					const Box3i box = Box3i(bpos << _block_size_pow2, Vector3i(_block_size))
											  .clipped(Box3i(min_pos, dst_buffer.get_size()));

					// TODO Format?
					VoxelBufferInternal temp;
					temp.create(box.size);
					gen_func(callback_data, temp, box.pos);

					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						dst_buffer.copy_from(temp, Vector3i(), temp.get_size(), box.pos - min_pos, channels[ci]);
					}

				} else {
					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						const uint8_t channel = channels[ci];
						// For now, inexistent blocks default to hardcoded defaults, corresponding to "empty space".
						// If we want to change this, we may have to add an API for that.
						dst_buffer.fill_area(
								_default_voxel[channel],
								src_block_origin - min_pos,
								src_block_origin - min_pos + block_size_v,
								channel);
					}
				}
			}
		}
	}
}

void VoxelDataMap::paste(Vector3i min_pos, VoxelBufferInternal &src_buffer, unsigned int channels_mask, bool use_mask,
		uint64_t mask_value, bool create_new_blocks) {
	//
	const Vector3i max_pos = min_pos + src_buffer.get_size();

	const Vector3i min_block_pos = voxel_to_block(min_pos);
	const Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1, 1, 1)) + Vector3i(1, 1, 1);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				for (unsigned int channel = 0; channel < VoxelBufferInternal::MAX_CHANNELS; ++channel) {
					if (((1 << channel) & channels_mask) == 0) {
						continue;
					}
					VoxelDataBlock *block = get_block(bpos);

					if (block == nullptr) {
						if (create_new_blocks) {
							block = create_default_block(bpos);
						} else {
							continue;
						}
					}

					const Vector3i dst_block_origin = block_to_voxel(bpos);

					VoxelBufferInternal &dst_buffer = block->get_voxels();
					RWLockWrite lock(dst_buffer.get_lock());

					if (use_mask) {
						const Box3i dst_box(min_pos - dst_block_origin, src_buffer.get_size());

						const Vector3i src_offset = -dst_box.pos;

						dst_buffer.read_write_action(dst_box, channel,
								[&src_buffer, mask_value, src_offset, channel](const Vector3i pos, uint64_t dst_v) {
									const uint64_t src_v = src_buffer.get_voxel(pos + src_offset, channel);
									if (src_v == mask_value) {
										return dst_v;
									}
									return src_v;
								});

					} else {
						dst_buffer.copy_from(src_buffer,
								Vector3i(),
								src_buffer.get_size(),
								min_pos - dst_block_origin,
								channel);
					}
				}
			}
		}
	}
}

void VoxelDataMap::clear() {
	for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
		VoxelDataBlock *block = *it;
		if (block == nullptr) {
			ERR_PRINT("Unexpected nullptr in VoxelMap::clear()");
		} else {
			memdelete(block);
		}
	}
	_blocks.clear();
	_blocks_map.clear();
}

int VoxelDataMap::get_block_count() const {
#ifdef DEBUG_ENABLED
	const unsigned int blocks_map_size = _blocks_map.size();
	CRASH_COND(_blocks.size() != blocks_map_size);
#endif
	return _blocks.size();
}

bool VoxelDataMap::is_area_fully_loaded(const Box3i voxels_box) const {
	Box3i block_box = voxels_box.downscaled(get_block_size());
	return block_box.all_cells_match([this](Vector3i pos) {
		return has_block(pos);
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void preload_box(VoxelDataLodMap &data, Box3i voxel_box, VoxelGenerator *generator) {
	VOXEL_PROFILE_SCOPE();
	//ERR_FAIL_COND_MSG(_full_load_mode == false, nullptr, "This function can only be used in full load mode");

	struct Task {
		Vector3i block_pos;
		unsigned int lod_index;
		std::shared_ptr<VoxelBufferInternal> voxels;
	};

	std::vector<Task> todo;
	// We'll pack tasks per LOD so we'll have less locking to do
	std::vector<unsigned int> count_per_lod;

	const unsigned int data_block_size = data.lods[0].map.get_block_size();

	// Find empty slots
	for (unsigned int lod_index = 0; lod_index < data.lod_count; ++lod_index) {
		const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

		PRINT_VERBOSE(String("Preloading box {0} at lod {1} synchronously")
							  .format(varray(block_box.to_string(), lod_index)));

		VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
		const unsigned int prev_size = todo.size();

		{
			RWLockRead rlock(data_lod.map_lock);
			block_box.for_each_cell([&data_lod, lod_index, &todo](Vector3i block_pos) {
				// We don't check "loading blocks", because this function wants to complete the task right now.
				if (!data_lod.map.has_block(block_pos)) {
					todo.push_back(Task{ block_pos, lod_index, nullptr });
				}
			});
		}

		count_per_lod.push_back(todo.size() - prev_size);
	}

	const Vector3i block_size(data_block_size);

	// Generate
	for (unsigned int i = 0; i < todo.size(); ++i) {
		Task &task = todo[i];
		task.voxels = gd_make_shared<VoxelBufferInternal>();
		task.voxels->create(block_size);
		// TODO Format?
		if (generator != nullptr) {
			VoxelBlockRequest r{
				*task.voxels,
				task.block_pos * (data_block_size << task.lod_index),
				int(task.lod_index)
			};
			generator->generate_block(r);
		}
	}

	// Populate slots
	unsigned int task_index = 0;
	for (unsigned int lod_index = 0; lod_index < data.lod_count; ++lod_index) {
		CRASH_COND(lod_index >= count_per_lod.size());
		const unsigned int count = count_per_lod[lod_index];

		if (count > 0) {
			const unsigned int end_task_index = task_index + count;

			VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			RWLockWrite wlock(data_lod.map_lock);

			for (; task_index < end_task_index; ++task_index) {
				Task &task = todo[task_index];
				CRASH_COND(task.lod_index != lod_index);
				if (data_lod.map.has_block(task.block_pos)) {
					// Sorry, that block has been set in the meantime by another thread.
					// We'll assume the block we just generated is redundant and discard it.
					continue;
				}
				data_lod.map.set_block_buffer(task.block_pos, task.voxels);
			}
		}
	}
}
