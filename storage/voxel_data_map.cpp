#include "voxel_data_map.h"
#include "../constants/cube_tables.h"
#include "../edition/funcs.h"
#include "../generators/voxel_generator.h"
#include "../util/containers/dynamic_bitset.h"
#include "../util/macros.h"
#include "../util/memory/memory.h"
#include "../util/string/format.h"

#include <limits>

namespace zylann::voxel {

VoxelDataMap::VoxelDataMap() {
	// This is not planned to change at runtime at the moment.
	// set_block_size_pow2(constants::DEFAULT_BLOCK_SIZE_PO2);
}

VoxelDataMap::~VoxelDataMap() {
	clear();
}

void VoxelDataMap::create(unsigned int lod_index) {
	ZN_ASSERT(lod_index < constants::MAX_LOD);
	clear();
	// set_block_size_pow2(block_size_po2);
	set_lod_index(lod_index);
}

// void VoxelDataMap::set_block_size_pow2(unsigned int p) {
// 	ZN_ASSERT_RETURN_MSG(p >= 1, "Block size is too small");
// 	ZN_ASSERT_RETURN_MSG(p <= 8, "Block size is too big");

// 	_block_size_pow2 = p;
// 	_block_size = 1 << _block_size_pow2;
// 	_block_size_mask = _block_size - 1;
// }

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
		return VoxelBuffer::get_default_value_static(c);
	}
	return block->get_voxels_const().get_voxel(to_local(pos), c);
}

VoxelDataBlock *VoxelDataMap::create_default_block(Vector3i bpos) {
	std::shared_ptr<VoxelBuffer> buffer = make_shared_instance<VoxelBuffer>(VoxelBuffer::ALLOCATOR_POOL);
	buffer->create(get_block_size(), get_block_size(), get_block_size());
	// buffer->set_default_values(_default_voxel);
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN_V(!has_block(bpos), nullptr);
#endif
	VoxelDataBlock &map_block = _blocks_map[bpos];
	map_block = VoxelDataBlock(buffer, _lod_index);
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
	VoxelBuffer &voxels = block->get_voxels();
	voxels.set_voxel(value, to_local(pos), c);
}

float VoxelDataMap::get_voxel_f(Vector3i pos, unsigned int c) const {
	Vector3i bpos = voxel_to_block(pos);
	const VoxelDataBlock *block = get_block(bpos);
	// TODO The generator needs to be invoked if the block has no voxels
	if (block == nullptr || !block->has_voxels()) {
		// TODO Not valid for a float return value
		return VoxelBuffer::get_default_value_static(c);
	}
	Vector3i lpos = to_local(pos);
	return block->get_voxels_const().get_voxel_f(lpos.x, lpos.y, lpos.z, c);
}

void VoxelDataMap::set_voxel_f(real_t value, Vector3i pos, unsigned int c) {
	VoxelDataBlock *block = get_or_create_block_at_voxel_pos(pos);
	Vector3i lpos = to_local(pos);
	// TODO In this situation, the generator must be invoked to fill the block
	ZN_ASSERT_RETURN_MSG(block->has_voxels(), "Block not cached");
	VoxelBuffer &voxels = block->get_voxels();
	voxels.set_voxel_f(value, lpos.x, lpos.y, lpos.z, c);
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

VoxelDataBlock *VoxelDataMap::set_block_buffer(Vector3i bpos, std::shared_ptr<VoxelBuffer> &buffer, bool overwrite) {
	ZN_ASSERT_RETURN_V(buffer != nullptr, nullptr);

	VoxelDataBlock *block = get_block(bpos);

	if (block == nullptr) {
		VoxelDataBlock &map_block = _blocks_map[bpos];
		map_block = VoxelDataBlock(buffer, _lod_index);
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

void VoxelDataMap::set_block(Vector3i bpos, const VoxelDataBlock &block) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT(block.get_lod_index() == _lod_index);
#endif
	_blocks_map[bpos] = block;
}

VoxelDataBlock *VoxelDataMap::set_empty_block(Vector3i bpos, bool overwrite) {
	VoxelDataBlock *block = get_block(bpos);

	if (block == nullptr) {
		VoxelDataBlock &map_block = _blocks_map[bpos];
		map_block = VoxelDataBlock(_lod_index);
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

void VoxelDataMap::copy(Vector3i min_pos, VoxelBuffer &dst_buffer, unsigned int channels_mask, void *callback_data,
		void (*gen_func)(void *, VoxelBuffer &, Vector3i)) const {
	// TODO Reimplement using `copy_from_chunked_storage`?

	ZN_ASSERT_RETURN_MSG(Vector3iUtil::get_volume(dst_buffer.get_size()) > 0, "The area to copy is empty");
	const Vector3i max_pos = min_pos + dst_buffer.get_size();

	const Vector3i min_block_pos = voxel_to_block(min_pos);
	const Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1, 1, 1)) + Vector3i(1, 1, 1);

	const Vector3i block_size_v(get_block_size(), get_block_size(), get_block_size());

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				const VoxelDataBlock *block = get_block(bpos);
				const Vector3i src_block_origin = block_to_voxel(bpos);

				if (block != nullptr && block->has_voxels()) {
					const VoxelBuffer &src_buffer = block->get_voxels_const();

					for (const uint8_t channel : channels) {
						dst_buffer.set_channel_depth(channel, src_buffer.get_channel_depth(channel));
						// Note: copy_from takes care of clamping the area if it's on an edge
						dst_buffer.copy_channel_from(
								src_buffer, min_pos - src_block_origin, src_buffer.get_size(), Vector3i(), channel);
					}

				} else if (gen_func != nullptr) {
					const Box3i box = Box3i(bpos << get_block_size_pow2(), block_size_v)
											  .clipped(Box3i(min_pos, dst_buffer.get_size()));

					// TODO Format?
					VoxelBuffer temp(VoxelBuffer::ALLOCATOR_POOL);
					temp.create(box.size);
					gen_func(callback_data, temp, box.position);

					for (const uint8_t channel : channels) {
						dst_buffer.copy_channel_from(
								temp, Vector3i(), temp.get_size(), box.position - min_pos, channel);
					}

				} else {
					for (const uint8_t channel : channels) {
						// For now, inexistent blocks default to hardcoded defaults, corresponding to "empty space".
						// If we want to change this, we may have to add an API for that.
						dst_buffer.fill_area(VoxelBuffer::get_default_value_static(channel), src_block_origin - min_pos,
								src_block_origin - min_pos + block_size_v, channel);
					}
				}
			}
		}
	}
}

void VoxelDataMap::paste(
		Vector3i min_pos, const VoxelBuffer &src_buffer, unsigned int channels_mask, bool create_new_blocks) {
	paste_masked(min_pos, src_buffer, channels_mask, false, 0, 0, false, 0, Span<const int32_t>(), create_new_blocks);
}

void VoxelDataMap::paste_masked( //
		Vector3i min_pos, //
		const VoxelBuffer &src_buffer, //
		unsigned int channels_mask, //
		bool use_src_mask, //
		uint8_t src_mask_channel, //
		uint64_t src_mask_value, //
		bool use_dst_mask, //
		uint8_t dst_mask_channel, //
		Span<const int32_t> dst_writable_values, //
		bool create_new_blocks //
) {
	//
	if (use_dst_mask && !use_src_mask) {
		ZN_PRINT_ERROR("Destination mask without source mask is not implemented");
		return;
	}

	// TODO Reimplement using `copy_to_chunked_storage`?
	//
	const Vector3i max_pos = min_pos + src_buffer.get_size();

	const Vector3i min_block_pos = voxel_to_block(min_pos);
	const Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1, 1, 1)) + Vector3i(1, 1, 1);

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channel_indices =
			VoxelBuffer::mask_to_channels_list(channels_mask);

	DynamicBitset bitarray;
	if (dst_writable_values.size() == 1) {
		ZN_ASSERT_RETURN(indices_to_bitarray_u16(dst_writable_values, bitarray));
	}

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {
				const bool with_metadata = true;

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

				VoxelBuffer &dst_buffer = block->get_voxels();
				const Vector3i dst_base_pos = min_pos - dst_block_origin;

				if (use_src_mask) {
					if (use_dst_mask) {
						if (dst_writable_values.size() == 1) {
							zylann::voxel::paste_src_masked_dst_writable_value(to_span(channel_indices),
									src_buffer, //
									src_mask_channel, //
									src_mask_value, //
									dst_buffer, //
									dst_base_pos, //
									dst_mask_channel, //
									dst_writable_values[0], //
									with_metadata //
							);

						} else {
							zylann::voxel::paste_src_masked_dst_writable_bitarray(to_span(channel_indices),
									src_buffer, //
									src_mask_channel, //
									src_mask_value, //
									dst_buffer, //
									dst_base_pos, //
									dst_mask_channel, //
									bitarray, //
									with_metadata //
							);
						}

					} else {
						zylann::voxel::paste_src_masked(to_span(channel_indices),
								src_buffer, //
								src_mask_channel, //
								src_mask_value, //
								dst_buffer, //
								dst_base_pos, //
								with_metadata //
						);
					}

				} else {
					zylann::voxel::paste(to_span(channel_indices), src_buffer, dst_buffer, dst_base_pos, with_metadata);
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

} // namespace zylann::voxel
