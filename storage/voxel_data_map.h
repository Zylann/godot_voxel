#ifndef VOXEL_DATA_MAP_H
#define VOXEL_DATA_MAP_H

#include "../util/fixed_array.h"
#include "../util/profiling.h"
#include "voxel_data_block.h"

#include <scene/main/node.h>
#include <unordered_map>

// Infinite voxel storage by means of octants like Gridmap, within a constant LOD.
// Convenience functions to access VoxelBuffers internally will lock them to protect against multithreaded access.
// However, the map itself is not thread-safe.
class VoxelDataMap {
public:
	// Converts voxel coodinates into block coordinates.
	// Don't use division because it introduces an offset in negative coordinates.
	static _FORCE_INLINE_ Vector3i voxel_to_block_b(Vector3i pos, int block_size_pow2) {
		return pos >> block_size_pow2;
	}

	_FORCE_INLINE_ Vector3i voxel_to_block(Vector3i pos) const {
		return voxel_to_block_b(pos, _block_size_pow2);
	}

	_FORCE_INLINE_ Vector3i to_local(Vector3i pos) const {
		return Vector3i(
				pos.x & _block_size_mask,
				pos.y & _block_size_mask,
				pos.z & _block_size_mask);
	}

	// Converts block coodinates into voxel coordinates
	_FORCE_INLINE_ Vector3i block_to_voxel(Vector3i bpos) const {
		return bpos * _block_size;
	}

	VoxelDataMap();
	~VoxelDataMap();

	void create(unsigned int block_size_po2, int lod_index);

	_FORCE_INLINE_ unsigned int get_block_size() const { return _block_size; }
	_FORCE_INLINE_ unsigned int get_block_size_pow2() const { return _block_size_pow2; }
	_FORCE_INLINE_ unsigned int get_block_size_mask() const { return _block_size_mask; }

	void set_lod_index(int lod_index);
	unsigned int get_lod_index() const;

	int get_voxel(Vector3i pos, unsigned int c = 0) const;
	void set_voxel(int value, Vector3i pos, unsigned int c = 0);

	float get_voxel_f(Vector3i pos, unsigned int c = VoxelBufferInternal::CHANNEL_SDF) const;
	void set_voxel_f(real_t value, Vector3i pos, unsigned int c = VoxelBufferInternal::CHANNEL_SDF);

	void set_default_voxel(int value, unsigned int channel = 0);
	int get_default_voxel(unsigned int channel = 0);

	// Gets a copy of all voxels in the area starting at min_pos having the same size as dst_buffer.
	void copy(Vector3i min_pos, VoxelBufferInternal &dst_buffer, unsigned int channels_mask) const;

	void paste(Vector3i min_pos, VoxelBufferInternal &src_buffer, unsigned int channels_mask, bool use_mask,
			uint64_t mask_value, bool create_new_blocks);

	// Moves the given buffer into a block of the map. The buffer is referenced, no copy is made.
	VoxelDataBlock *set_block_buffer(Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer);

	struct NoAction {
		inline void operator()(VoxelDataBlock *block) {}
	};

	template <typename Action_T>
	void remove_block(Vector3i bpos, Action_T pre_delete) {
		if (_last_accessed_block && _last_accessed_block->position == bpos) {
			_last_accessed_block = nullptr;
		}
		auto it = _blocks_map.find(bpos);
		if (it != _blocks_map.end()) {
			const unsigned int i = it->second;
#ifdef DEBUG_ENABLED
			CRASH_COND(i >= _blocks.size());
#endif
			VoxelDataBlock *block = _blocks[i];
			ERR_FAIL_COND(block == nullptr);
			pre_delete(block);
			memdelete(block);
			remove_block_internal(bpos, i);
		}
	}

	VoxelDataBlock *get_block(Vector3i bpos);
	const VoxelDataBlock *get_block(Vector3i bpos) const;

	bool has_block(Vector3i pos) const;
	bool is_block_surrounded(Vector3i pos) const;

	void clear();

	int get_block_count() const;

	// TODO Rename for_each_block
	template <typename Op_T>
	inline void for_all_blocks(Op_T op) {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			op(*it);
		}
	}

	// TODO Rename for_each_block
	template <typename Op_T>
	inline void for_all_blocks(Op_T op) const {
		for (auto it = _blocks.begin(); it != _blocks.end(); ++it) {
			op(*it);
		}
	}

	bool is_area_fully_loaded(const Box3i voxels_box) const;

	// D action(Vector3i pos, D value)
	template <typename F>
	void write_box(const Box3i &voxel_box, unsigned int channel, F action) {
		const Box3i block_box = voxel_box.downscaled(get_block_size());
		const Vector3i block_size(get_block_size());
		block_box.for_each_cell_zxy([this, action, voxel_box, channel, block_size](Vector3i block_pos) {
			VoxelDataBlock *block = get_block(block_pos);
			if (block != nullptr) {
				const Vector3i block_origin = block_to_voxel(block_pos);
				Box3i local_box(voxel_box.pos - block_origin, voxel_box.size);
				local_box.clip(Box3i(Vector3i(), block_size));
				block->get_voxels().write_box(local_box, channel, action, block_origin);
			}
		});
	}

	// action(Vector3i pos, D0 &value, D1 &value)
	template <typename F>
	void write_box_2(const Box3i &voxel_box, unsigned int channel0, unsigned int channel1, F action) {
		const Box3i block_box = voxel_box.downscaled(get_block_size());
		const Vector3i block_size(get_block_size());
		block_box.for_each_cell_zxy([this, action, voxel_box, channel0, channel1, block_size](Vector3i block_pos) {
			VoxelDataBlock *block = get_block(block_pos);
			if (block != nullptr) {
				const Vector3i block_origin = block_to_voxel(block_pos);
				Box3i local_box(voxel_box.pos - block_origin, voxel_box.size);
				local_box.clip(Box3i(Vector3i(), block_size));
				block->get_voxels().write_box_2_template<F, uint16_t, uint16_t>(
						local_box, channel0, channel1, action, block_origin);
			}
		});
	}

private:
	void set_block(Vector3i bpos, VoxelDataBlock *block);
	VoxelDataBlock *get_or_create_block_at_voxel_pos(Vector3i pos);
	VoxelDataBlock *create_default_block(Vector3i bpos);
	void remove_block_internal(Vector3i bpos, unsigned int index);

	void set_block_size_pow2(unsigned int p);

private:
	// Voxel values that will be returned if access is out of map bounds
	FixedArray<uint64_t, VoxelBufferInternal::MAX_CHANNELS> _default_voxel;

	// Blocks stored with a spatial hash in all 3D directions.
	// Before I used Godot's HashMap with RELATIONSHIP = 2 because that delivers better performance compared to
	// defaults, but it sometimes has very long stalls on removal, which std::unordered_map doesn't seem to have
	// (not as badly). Also overall performance is slightly better.
	std::unordered_map<Vector3i, unsigned int> _blocks_map;
	std::vector<VoxelDataBlock *> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	mutable VoxelDataBlock *_last_accessed_block;

	unsigned int _block_size;
	unsigned int _block_size_pow2;
	unsigned int _block_size_mask;

	unsigned int _lod_index = 0;
};

#endif // VOXEL_MAP_H
