#include "voxel_data_map.h"
#include "../constants/cube_tables.h"
#include "../generators/voxel_generator.h"
#include "../util/macros.h"
#include "../util/memory.h"
#include "../util/string_funcs.h"

#include <limits>

namespace zylann::voxel {

VoxelDataMap::VoxelDataMap() {
	// TODO Make it configurable in editor (with all necessary notifications and updatings!)
	set_block_size_pow2(constants::DEFAULT_BLOCK_SIZE_PO2);

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
	ZN_ASSERT_RETURN_MSG(p >= 1, "Block size is too small");
	ZN_ASSERT_RETURN_MSG(p <= 8, "Block size is too big");

	_block_size_pow2 = p;
	_block_size = 1 << _block_size_pow2;
	_block_size_mask = _block_size - 1;
}

void VoxelDataMap::set_lod_index(int lod_index) {
	ZN_ASSERT_RETURN_MSG(lod_index >= 0, "LOD index can't be negative");
	ZN_ASSERT_RETURN_MSG(lod_index < 32, "LOD index is too big");

	_lod_index = lod_index;
}

unsigned int VoxelDataMap::get_lod_index() const {
	return _lod_index;
}

int VoxelDataMap::get_voxel(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelDataBlock *block = get_block(bpos);
	if (block == nullptr || !block->has_voxels()) {
		return _default_voxel[c];
	}
	RWLockRead lock(block->get_voxels_const().get_lock());
	return block->get_voxels_const().get_voxel(to_local(pos), c);
}

VoxelDataBlock *VoxelDataMap::create_default_block(Vector3i bpos) {
	std::shared_ptr<VoxelBufferInternal> buffer = make_shared_instance<VoxelBufferInternal>();
	buffer->create(_block_size, _block_size, _block_size);
	buffer->set_default_values(_default_voxel);
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(!has_block(bpos), nullptr);
#endif
	VoxelDataBlock &map_block = _blocks_map[bpos];
	// TODO Clang complains the `move` prevents copy elision.
	// But I dont want `VoxelDataBlock` to have copy... so what, should I add [expensive] copy construction just so
	// clang is able to elide it?
	map_block = std::move(VoxelDataBlock(buffer, _lod_index));
	return &map_block;
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
	// TODO The generator needs to be invoked if the block has no voxels
	if (block == nullptr || !block->has_voxels()) {
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
	// TODO In this situation, the generator must be invoked to fill the block
	ZN_ASSERT_RETURN_MSG(block->has_voxels(), "Block not cached");
	VoxelBufferInternal &voxels = block->get_voxels();
	RWLockWrite lock(voxels.get_lock());
	voxels.set_voxel_f(value, lpos.x, lpos.y, lpos.z, c);
}

void VoxelDataMap::set_default_voxel(int value, unsigned int channel) {
	ZN_ASSERT_RETURN(channel >= 0 && channel < VoxelBufferInternal::MAX_CHANNELS);
	_default_voxel[channel] = value;
}

int VoxelDataMap::get_default_voxel(unsigned int channel) {
	ZN_ASSERT_RETURN_V(channel >= 0 && channel < VoxelBufferInternal::MAX_CHANNELS, 0);
	return _default_voxel[channel];
}

VoxelDataBlock *VoxelDataMap::get_block(Vector3i bpos) {
	auto it = _blocks_map.find(bpos);
	if (it != _blocks_map.end()) {
		return &it->second;
	}
	return nullptr;
}

const VoxelDataBlock *VoxelDataMap::get_block(Vector3i bpos) const {
	auto it = _blocks_map.find(bpos);
	if (it != _blocks_map.end()) {
		return &it->second;
	}
	return nullptr;
}

VoxelDataBlock *VoxelDataMap::set_block_buffer(
		Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer, bool overwrite) {
	ZN_ASSERT_RETURN_V(buffer != nullptr, nullptr);

	VoxelDataBlock *block = get_block(bpos);

	if (block == nullptr) {
		VoxelDataBlock &map_block = _blocks_map[bpos];
		map_block = std::move(VoxelDataBlock(buffer, _lod_index));
		block = &map_block;

	} else if (overwrite) {
		block->set_voxels(buffer);

	} else {
		ZN_PROFILE_MESSAGE("Redundant data block");
		ZN_PRINT_VERBOSE(format(
				"Discarded block {} lod {}, there was already data and overwriting is not enabled", bpos, _lod_index));
	}

	return block;
}

VoxelDataBlock *VoxelDataMap::set_empty_block(Vector3i bpos, bool overwrite) {
	VoxelDataBlock *block = get_block(bpos);

	if (block == nullptr) {
		VoxelDataBlock &map_block = _blocks_map[bpos];
		map_block = std::move(VoxelDataBlock(_lod_index));
		block = &map_block;

	} else if (overwrite) {
		block->clear_voxels();

	} else {
		ZN_PROFILE_MESSAGE("Redundant data block");
		ZN_PRINT_VERBOSE(format(
				"Discarded block {} lod {}, there was already data and overwriting is not enabled", bpos, _lod_index));
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

				if (block != nullptr && block->has_voxels()) {
					const VoxelBufferInternal &src_buffer = block->get_voxels_const();

					RWLockRead rlock(src_buffer.get_lock());

					for (unsigned int ci = 0; ci < channels_count; ++ci) {
						const uint8_t channel = channels[ci];
						dst_buffer.set_channel_depth(channel, src_buffer.get_channel_depth(channel));
						// Note: copy_from takes care of clamping the area if it's on an edge
						dst_buffer.copy_from(
								src_buffer, min_pos - src_block_origin, src_buffer.get_size(), Vector3i(), channel);
					}

				} else if (gen_func != nullptr) {
					const Box3i box = Box3i(bpos << _block_size_pow2, Vector3iUtil::create(_block_size))
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
						dst_buffer.fill_area(_default_voxel[channel], src_block_origin - min_pos,
								src_block_origin - min_pos + block_size_v, channel);
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

					// TODO In this situation, the generator has to be invoked to fill the blanks
					ZN_ASSERT_CONTINUE_MSG(block->has_voxels(), "Area not cached");

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
						dst_buffer.copy_from(
								src_buffer, Vector3i(), src_buffer.get_size(), min_pos - dst_block_origin, channel);
					}
				}
			}
		}
	}
}

void VoxelDataMap::clear() {
	_blocks_map.clear();
}

int VoxelDataMap::get_block_count() const {
	return _blocks_map.size();
}

bool VoxelDataMap::is_area_fully_loaded(const Box3i voxels_box) const {
	Box3i block_box = voxels_box.downscaled(get_block_size());
	return block_box.all_cells_match([this](Vector3i pos) { //
		return has_block(pos);
	});
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void preload_box(VoxelDataLodMap &data, Box3i voxel_box, VoxelGenerator *generator, bool is_streaming) {
	ZN_PROFILE_SCOPE();
	//ERR_FAIL_COND_MSG(_full_load_mode == false, nullptr, "This function can only be used in full load mode");

	struct Task {
		Vector3i block_pos;
		uint32_t lod_index;
		std::shared_ptr<VoxelBufferInternal> voxels;
	};

	std::vector<Task> todo;
	// We'll pack tasks per LOD so we'll have less locking to do
	std::vector<unsigned int> count_per_lod;

	const unsigned int data_block_size = data.lods[0].map.get_block_size();

	// Find empty slots
	for (unsigned int lod_index = 0; lod_index < data.lod_count; ++lod_index) {
		const Box3i block_box = voxel_box.downscaled(data_block_size << lod_index);

		ZN_PRINT_VERBOSE(format("Preloading box {} at lod {} synchronously", block_box, lod_index));

		VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
		const unsigned int prev_size = todo.size();

		{
			RWLockRead rlock(data_lod.map_lock);
			block_box.for_each_cell([&data_lod, lod_index, &todo, is_streaming](Vector3i block_pos) {
				// We don't check "loading blocks", because this function wants to complete the task right now.
				const VoxelDataBlock *block = data_lod.map.get_block(block_pos);
				if (is_streaming) {
					// Non-resident blocks must not be touched because we don't know what's in them.
					// We can generate caches if resident ones have no voxel data.
					if (block != nullptr && !block->has_voxels()) {
						todo.push_back(Task{ block_pos, lod_index, nullptr });
					}
				} else {
					// We can generate anywhere voxel data is not in memory
					if (block == nullptr || !block->has_voxels()) {
						todo.push_back(Task{ block_pos, lod_index, nullptr });
					}
				}
			});
		}

		count_per_lod.push_back(todo.size() - prev_size);
	}

	const Vector3i block_size = Vector3iUtil::create(data_block_size);

	// Generate
	for (unsigned int i = 0; i < todo.size(); ++i) {
		Task &task = todo[i];
		task.voxels = make_shared_instance<VoxelBufferInternal>();
		task.voxels->create(block_size);
		// TODO Format?
		if (generator != nullptr) {
			VoxelGenerator::VoxelQueryData q{ *task.voxels, task.block_pos * (data_block_size << task.lod_index),
				task.lod_index };
			generator->generate_block(q);
			data.modifiers.apply(q.voxel_buffer, AABB(q.origin_in_voxels, q.voxel_buffer.get_size() << q.lod));
		}
	}

	// Populate slots
	unsigned int task_index = 0;
	for (unsigned int lod_index = 0; lod_index < data.lod_count; ++lod_index) {
		ZN_ASSERT(lod_index < count_per_lod.size());
		const unsigned int count = count_per_lod[lod_index];

		if (count > 0) {
			const unsigned int end_task_index = task_index + count;

			VoxelDataLodMap::Lod &data_lod = data.lods[lod_index];
			RWLockWrite wlock(data_lod.map_lock);

			for (; task_index < end_task_index; ++task_index) {
				Task &task = todo[task_index];
				ZN_ASSERT(task.lod_index == lod_index);
				const VoxelDataBlock *prev_block = data_lod.map.get_block(task.block_pos);
				if (prev_block != nullptr && prev_block->has_voxels()) {
					// Sorry, that block has been set in the meantime by another thread.
					// We'll assume the block we just generated is redundant and discard it.
					continue;
				}
				data_lod.map.set_block_buffer(task.block_pos, task.voxels, true);
			}
		}
	}
}

void clear_cached_blocks_in_voxel_area(VoxelDataLodMap &data, Box3i p_voxel_box) {
	for (unsigned int lod_index = 0; lod_index < data.lod_count; ++lod_index) {
		VoxelDataLodMap::Lod &lod = data.lods[lod_index];
		RWLockRead rlock(lod.map_lock);

		const Box3i blocks_box = p_voxel_box.downscaled(lod.map.get_block_size() << lod_index);
		blocks_box.for_each_cell_zxy([&lod](const Vector3i bpos) {
			VoxelDataBlock *block = lod.map.get_block(bpos);
			if (block == nullptr || block->is_edited() || block->is_modified()) {
				return;
			}
			block->clear_voxels();
		});
	}
}

} // namespace zylann::voxel
