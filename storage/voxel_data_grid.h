#ifndef VOXEL_DATA_GRID_H
#define VOXEL_DATA_GRID_H

#include "voxel_data_map.h"

// Stores blocks of voxel data in a finite grid.
// This is used as temporary storage for some operations, to avoid holding exclusive locks on maps for too long.
class VoxelDataGrid {
public:
	// Rebuilds the grid and caches blocks intersecting the specified voxel box.
	inline void reference_area(VoxelDataMap &map, Box3i voxel_box) {
		const Box3i blocks_box = voxel_box.downscaled(map.get_block_size());
		create(blocks_box.size, map.get_block_size());
		_offset_in_blocks = blocks_box.pos;
		blocks_box.for_each_cell_zxy([&map, this](const Vector3i pos) {
			VoxelDataBlock *block = map.get_block(pos);
			if (block != nullptr) {
				set_block(pos, block->get_voxels_shared());
			} else {
				set_block(pos, nullptr);
			}
		});
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
		_box_loop(voxel_box, [action, channel0, channel1](
									 VoxelBufferInternal &voxels, Box3i local_box, Vector3i voxel_offset) {
			voxels.write_box_2_template<F, uint16_t, uint16_t>(local_box, channel0, channel1, action, voxel_offset);
		});
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
					local_box.clip(Box3i(Vector3i(), Vector3i(_block_size)));
					RWLockWrite wlock(block->get_lock());
					block_action(*block, local_box, block_origin);
				}
			}
		}
	}

	inline void clear() {
		_blocks.clear();
		_size_in_blocks = Vector3i();
	}

	inline void create(Vector3i size, unsigned int block_size) {
		_blocks.clear();
		_blocks.resize(size.volume());
		_size_in_blocks = size;
		_block_size = block_size;
	}

	inline bool is_valid_position(Vector3i pos) {
		pos -= _offset_in_blocks;
		return pos.x >= 0 &&
			   pos.y >= 0 &&
			   pos.z >= 0 &&
			   pos.x < _size_in_blocks.x &&
			   pos.y < _size_in_blocks.y &&
			   pos.z < _size_in_blocks.z;
	}

	inline void set_block(Vector3i position, std::shared_ptr<VoxelBufferInternal> block) {
		ERR_FAIL_COND(!is_valid_position(position));
		position -= _offset_in_blocks;
		const unsigned int index = position.get_zxy_index(_size_in_blocks);
		CRASH_COND(index >= _blocks.size());
		_blocks[index] = block;
	}

	inline VoxelBufferInternal *get_block(Vector3i position) {
		ERR_FAIL_COND_V(!is_valid_position(position), nullptr);
		position -= _offset_in_blocks;
		const unsigned int index = position.get_zxy_index(_size_in_blocks);
		CRASH_COND(index >= _blocks.size());
		return _blocks[index].get();
	}

	// Flat grid indexed in ZXY order
	std::vector<std::shared_ptr<VoxelBufferInternal>> _blocks;
	// Size of the grid in blocks
	Vector3i _size_in_blocks;
	// Block coordinates offset. This is used for when we cache a sub-region of a map, we need to keep the origin
	// of the area in memory so we can keep using the same coordinate space
	Vector3i _offset_in_blocks;
	// Size of a block in voxels
	unsigned int _block_size = 1 << VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;
};

#endif // VOXEL_DATA_GRID_H
