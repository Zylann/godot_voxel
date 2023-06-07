#ifndef VOXEL_DATA_MAP_H
#define VOXEL_DATA_MAP_H

#include "../util/fixed_array.h"
#include "../util/profiling.h"
#include "voxel_data_block.h"

#include <unordered_map>

namespace zylann::voxel {

class VoxelGenerator;

// Sparse voxel storage by means of cubic chunks, within a constant LOD.
//
// Convenience functions to access VoxelBuffers internally will lock them to protect against multithreaded access.
// However, the map itself is not thread-safe.
//
// When doing data streaming, the volume is *partially* loaded. If a block is not found at some coordinates,
// it means we don't know if it contains edits or not. Knowing this is important to avoid writing or caching voxel data
// in blank areas, that may be completely different once loaded.
// When using "full load" of edits, it doesn't matter. If all edits are loaded, we know up-front that everything else
// isn't edited (which also means we may not find blocks without data in them).
//
class VoxelDataMap {
public:
	// This is block size in VOXELS. To convert to space units, use `block_size << lod_index`.
	static const unsigned int BLOCK_SIZE_PO2 = constants::DEFAULT_BLOCK_SIZE_PO2;
	static const unsigned int BLOCK_SIZE = 1 << BLOCK_SIZE_PO2;
	static const unsigned int BLOCK_SIZE_MASK = BLOCK_SIZE - 1;

	// Converts voxel coordinates into block coordinates.
	// Don't use division because it introduces an offset in negative coordinates.
	static inline Vector3i voxel_to_block_b(Vector3i pos, int block_size_pow2) {
		return pos >> block_size_pow2;
	}

	inline Vector3i voxel_to_block(Vector3i pos) const {
		return voxel_to_block_b(pos, BLOCK_SIZE_PO2);
	}

	inline Vector3i to_local(Vector3i pos) const {
		return Vector3i(pos.x & BLOCK_SIZE_MASK, pos.y & BLOCK_SIZE_MASK, pos.z & BLOCK_SIZE_MASK);
	}

	// Converts block coordinates into voxel coordinates.
	inline Vector3i block_to_voxel(Vector3i bpos) const {
		return bpos * BLOCK_SIZE;
	}

	VoxelDataMap();
	~VoxelDataMap();

	void create(unsigned int lod_index);

	inline unsigned int get_block_size() const {
		return BLOCK_SIZE;
	}
	inline unsigned int get_block_size_pow2() const {
		return BLOCK_SIZE_PO2;
	}
	inline unsigned int get_block_size_mask() const {
		return BLOCK_SIZE_MASK;
	}

	void set_lod_index(int lod_index);
	unsigned int get_lod_index() const;

	int get_voxel(Vector3i pos, unsigned int c = 0) const;
	void set_voxel(int value, Vector3i pos, unsigned int c = 0);

	float get_voxel_f(Vector3i pos, unsigned int c = VoxelBufferInternal::CHANNEL_SDF) const;
	void set_voxel_f(real_t value, Vector3i pos, unsigned int c = VoxelBufferInternal::CHANNEL_SDF);

	inline void copy(Vector3i min_pos, VoxelBufferInternal &dst_buffer, unsigned int channels_mask) const {
		copy(min_pos, dst_buffer, channels_mask, nullptr, nullptr);
	}

	// Gets a copy of all voxels in the area starting at min_pos having the same size as dst_buffer.
	void copy(Vector3i min_pos, VoxelBufferInternal &dst_buffer, unsigned int channels_mask, void *,
			void (*gen_func)(void *, VoxelBufferInternal &, Vector3i)) const;

	void paste(Vector3i min_pos, const VoxelBufferInternal &src_buffer, unsigned int channels_mask, bool use_mask,
			uint8_t mask_channel, uint64_t mask_value, bool create_new_blocks);

	// Moves the given buffer into a block of the map. The buffer is referenced, no copy is made.
	VoxelDataBlock *set_block_buffer(Vector3i bpos, std::shared_ptr<VoxelBufferInternal> &buffer, bool overwrite);
	VoxelDataBlock *set_empty_block(Vector3i bpos, bool overwrite);
	void set_block(Vector3i bpos, const VoxelDataBlock &block);

	struct NoAction {
		inline void operator()(VoxelDataBlock &block) {}
	};

	template <typename Action_T>
	void remove_block(Vector3i bpos, Action_T pre_delete) {
		auto it = _blocks_map.find(bpos);
		if (it != _blocks_map.end()) {
			pre_delete(it->second);
			_blocks_map.erase(it);
		}
	}

	VoxelDataBlock *get_block(Vector3i bpos);
	const VoxelDataBlock *get_block(Vector3i bpos) const;

	bool has_block(Vector3i pos) const;
	bool is_block_surrounded(Vector3i pos) const;

	void clear();

	int get_block_count() const;

	// op(Vector3i bpos, VoxelDataBlock &block)
	template <typename Op_T>
	inline void for_each_block(Op_T op) {
		for (auto it = _blocks_map.begin(); it != _blocks_map.end(); ++it) {
			op(it->first, it->second);
		}
	}

	// void op(Vector3i bpos, const VoxelDataBlock &block)
	template <typename Op_T>
	inline void for_each_block(Op_T op) const {
		for (auto it = _blocks_map.begin(); it != _blocks_map.end(); ++it) {
			op(it->first, it->second);
		}
	}

	bool is_area_fully_loaded(const Box3i voxels_box) const;

	template <typename F>
	inline void write_box(const Box3i &voxel_box, unsigned int channel, F action) {
		write_box(voxel_box, channel, action, [](const VoxelBufferInternal &, const Vector3i &) {});
	}

	// D F(Vector3i pos, D value)
	template <typename F, typename G>
	void write_box(const Box3i &voxel_box, unsigned int channel, F action, G gen_func) {
		const Box3i block_box = voxel_box.downscaled(get_block_size());
		const Vector3i block_size = Vector3iUtil::create(get_block_size());
		block_box.for_each_cell_zxy([this, action, voxel_box, channel, block_size, gen_func](Vector3i block_pos) {
			VoxelDataBlock *block = get_block(block_pos);
			if (block == nullptr) {
				ZN_PROFILE_SCOPE_NAMED("Generate");
				block = create_default_block(block_pos);
				gen_func(block->get_voxels(), block_pos << get_block_size_pow2());
			}
			const Vector3i block_origin = block_to_voxel(block_pos);
			Box3i local_box(voxel_box.pos - block_origin, voxel_box.size);
			local_box.clip(Box3i(Vector3i(), block_size));
			RWLockWrite wlock(block->get_voxels().get_lock());
			block->get_voxels().write_box(local_box, channel, action, block_origin);
		});
	}

	template <typename F>
	inline void write_box_2(const Box3i &voxel_box, unsigned int channel0, unsigned int channel1, F action) {
		write_box_2(voxel_box, channel0, channel1, action, [](const VoxelBufferInternal &, const Vector3i &) {});
	}

	// void F(Vector3i pos, D0 &value, D1 &value)
	template <typename F, typename G>
	void write_box_2(const Box3i &voxel_box, unsigned int channel0, unsigned int channel1, F action, G gen_func) {
		const Box3i block_box = voxel_box.downscaled(get_block_size());
		const Vector3i block_size = Vector3iUtil::create(get_block_size());
		block_box.for_each_cell_zxy(
				[this, action, voxel_box, channel0, channel1, block_size, gen_func](Vector3i block_pos) {
					VoxelDataBlock *block = get_block(block_pos);
					if (block == nullptr) {
						block = create_default_block(block_pos);
						gen_func(block->get_voxels(), block_pos << get_block_size_pow2());
					}
					const Vector3i block_origin = block_to_voxel(block_pos);
					Box3i local_box(voxel_box.pos - block_origin, voxel_box.size);
					local_box.clip(Box3i(Vector3i(), block_size));
					RWLockWrite wlock(block->get_voxels().get_lock());
					block->get_voxels().write_box_2_template<F, uint16_t, uint16_t>(
							local_box, channel0, channel1, action, block_origin);
				});
	}

private:
	// void set_block(Vector3i bpos, VoxelDataBlock *block);
	VoxelDataBlock *get_or_create_block_at_voxel_pos(Vector3i pos);
	VoxelDataBlock *create_default_block(Vector3i bpos);

	// void set_block_size_pow2(unsigned int p);

private:
	// Blocks stored with a spatial hash in all 3D directions.
	// Before I used Godot 3's HashMap with RELATIONSHIP = 2 because that delivers better performance compared to
	// defaults, but it sometimes has very long stalls on removal, which std::unordered_map doesn't seem to have
	// (not as badly). Also overall performance is slightly better.
	// Note: pointers to elements remain valid when inserting or removing others (only iterators may be invalidated)
	std::unordered_map<Vector3i, VoxelDataBlock> _blocks_map;

	// This was a possible optimization in a single-threaded scenario, but it's not in multithread.
	// We want to be able to do shared read-accesses but this is a mutable variable.
	// If we want this back, it may be thread-local in some way.
	//
	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	// mutable VoxelDataBlock *_last_accessed_block = nullptr;

	unsigned int _lod_index = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_MAP_H
