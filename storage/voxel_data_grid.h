#ifndef VOXEL_DATA_GRID_H
#define VOXEL_DATA_GRID_H

#include "voxel_data_map.h"

namespace zylann::voxel {

// Stores blocks of voxel data in a finite grid.
// This is used as temporary storage for some operations, to avoid holding exclusive locks on maps for too long.
// TODO Have a readonly version to enforce no writes?
class VoxelDataGrid {
public:
	// Rebuilds the grid and caches blocks intersecting the specified voxel box.
	// Warning: the given box is in voxels RELATIVE to the passed map. It that map is not LOD0, you may downscale the
	// box if you expect LOD0 coordinates.
	inline void reference_area(const VoxelDataMap &map, Box3i voxel_box) {
		ZN_PROFILE_SCOPE();
		const Box3i blocks_box = voxel_box.downscaled(map.get_block_size());
		create(blocks_box.size, map.get_block_size());
		_offset_in_blocks = blocks_box.pos;
		blocks_box.for_each_cell_zxy([&map, this](const Vector3i pos) {
			const VoxelDataBlock *block = map.get_block(pos);
			// TODO Might need to invoke the generator at some level for present blocks without voxels,
			// or make sure all blocks contain voxel data
			if (block != nullptr && block->has_voxels()) {
				set_block(pos, block->get_voxels_shared());
			} else {
				set_block(pos, nullptr);
			}
		});
	}

	inline bool has_any_block() const {
		for (unsigned int i = 0; i < _blocks.size(); ++i) {
			if (_blocks[i] != nullptr) {
				return true;
			}
		}
		return false;
	}

	inline bool try_get_voxel_f(Vector3i pos, float &out_value, VoxelBufferInternal::ChannelId channel) const {
		const Vector3i bpos = (pos >> _block_size_po2) - _offset_in_blocks;
		if (!is_valid_relative_block_position(bpos)) {
			return false;
		}
		const unsigned int loc = Vector3iUtil::get_zxy_index(bpos, _size_in_blocks);
		const VoxelBufferInternal *voxels = _blocks[loc].get();
		if (voxels == nullptr) {
			return false;
		}
		const unsigned int mask = (1 << _block_size_po2) - 1;
		const Vector3i rpos = pos & mask;
		{
			RWLockRead rlock(voxels->get_lock());
			out_value = voxels->get_voxel_f(rpos, channel);
		}
		return true;
	}

	// D action(Vector3i pos, D value)
	template <typename F>
	void write_box(Box3i voxel_box, unsigned int channel, F action) {
		_box_loop(voxel_box, [action, channel](VoxelBufferInternal &voxels, Box3i local_box, Vector3i voxel_offset) {
			voxels.write_box(local_box, channel, action, voxel_offset);
		});
	}

	// void action(Vector3i pos, D0 &value, D1 &value)
	template <typename F>
	void write_box_2(const Box3i &voxel_box, unsigned int channel0, unsigned int channel1, F action) {
		_box_loop(voxel_box,
				[action, channel0, channel1](VoxelBufferInternal &voxels, Box3i local_box, Vector3i voxel_offset) {
					voxels.write_box_2_template<F, uint16_t, uint16_t>(
							local_box, channel0, channel1, action, voxel_offset);
				});
	}

	// inline const VoxelBufferInternal *get_block(Vector3i position) const {
	// 	ERR_FAIL_COND_V(!is_valid_position(position), nullptr);
	// 	position -= _offset_in_blocks;
	// 	const unsigned int index = Vector3iUtil::get_zxy_index(position, _size_in_blocks);
	// 	CRASH_COND(index >= _blocks.size());
	// 	return _blocks[index].get();
	// }

	inline void clear() {
		_blocks.clear();
		_size_in_blocks = Vector3i();
	}

private:
	inline unsigned int get_block_size() const {
		return _block_size;
	}

	template <typename Block_F>
	inline void _box_loop(Box3i voxel_box, Block_F block_action) {
		Vector3i block_rpos;
		const Vector3i area_origin_in_voxels = _offset_in_blocks * _block_size;
		unsigned int index = 0;
		for (block_rpos.z = 0; block_rpos.z < _size_in_blocks.z; ++block_rpos.z) {
			for (block_rpos.x = 0; block_rpos.x < _size_in_blocks.x; ++block_rpos.x) {
				for (block_rpos.y = 0; block_rpos.y < _size_in_blocks.y; ++block_rpos.y) {
					VoxelBufferInternal *block = _blocks[index].get();
					// Flat grid and iteration order allows us to just increment the index since we iterate them all
					++index;
					if (block == nullptr) {
						continue;
					}
					const Vector3i block_origin = block_rpos * _block_size + area_origin_in_voxels;
					Box3i local_box(voxel_box.pos - block_origin, voxel_box.size);
					local_box.clip(Box3i(Vector3i(), Vector3iUtil::create(_block_size)));
					RWLockWrite wlock(block->get_lock());
					block_action(*block, local_box, block_origin);
				}
			}
		}
	}

	inline void create(Vector3i size, unsigned int block_size) {
		ZN_PROFILE_SCOPE();
		_blocks.clear();
		_blocks.resize(Vector3iUtil::get_volume(size));
		_size_in_blocks = size;
		_block_size = block_size;
	}

	inline bool is_valid_relative_block_position(Vector3i pos) const {
		return pos.x >= 0 && //
				pos.y >= 0 && //
				pos.z >= 0 && //
				pos.x < _size_in_blocks.x && //
				pos.y < _size_in_blocks.y && //
				pos.z < _size_in_blocks.z;
	}

	inline bool is_valid_block_position(Vector3i pos) const {
		return is_valid_relative_block_position(pos - _offset_in_blocks);
	}

	inline void set_block(Vector3i position, std::shared_ptr<VoxelBufferInternal> block) {
		ZN_ASSERT_RETURN(is_valid_block_position(position));
		position -= _offset_in_blocks;
		const unsigned int index = Vector3iUtil::get_zxy_index(position, _size_in_blocks);
		ZN_ASSERT(index < _blocks.size());
		_blocks[index] = block;
	}

	inline VoxelBufferInternal *get_block(Vector3i position) {
		ZN_ASSERT_RETURN_V(is_valid_block_position(position), nullptr);
		position -= _offset_in_blocks;
		const unsigned int index = Vector3iUtil::get_zxy_index(position, _size_in_blocks);
		ZN_ASSERT(index < _blocks.size());
		return _blocks[index].get();
	}

	// Flat grid indexed in ZXY order
	// TODO Ability to use thread-local/stack pool allocator? Such grids are often temporary
	std::vector<std::shared_ptr<VoxelBufferInternal>> _blocks;
	// Size of the grid in blocks
	Vector3i _size_in_blocks;
	// Block coordinates offset. This is used for when we cache a sub-region of a map, we need to keep the origin
	// of the area in memory so we can keep using the same coordinate space
	Vector3i _offset_in_blocks;
	// Size of a block in voxels
	unsigned int _block_size_po2 = constants::DEFAULT_BLOCK_SIZE_PO2;
	unsigned int _block_size = 1 << constants::DEFAULT_BLOCK_SIZE_PO2;
};

} // namespace zylann::voxel

#endif // VOXEL_DATA_GRID_H
