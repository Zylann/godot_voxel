#ifndef VOXEL_MAP_H
#define VOXEL_MAP_H

#include "voxel_block.h"

#include <core/hash_map.h>
#include <scene/main/node.h>

// Infinite voxel storage by means of octants like Gridmap, within a constant LOD
class VoxelMap : public Reference {
	GDCLASS(VoxelMap, Reference)
public:
	// Converts voxel coodinates into block coordinates.
	// Don't use division because it introduces an offset in negative coordinates.
	static _FORCE_INLINE_ Vector3i voxel_to_block_b(Vector3i pos, int block_size_pow2) {
		return Vector3i(
				pos.x >> block_size_pow2,
				pos.y >> block_size_pow2,
				pos.z >> block_size_pow2);
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

	VoxelMap();
	~VoxelMap();

	_FORCE_INLINE_ unsigned int get_block_size() const { return _block_size; }
	_FORCE_INLINE_ unsigned int get_block_size_pow2() const { return _block_size_pow2; }
	_FORCE_INLINE_ unsigned int get_block_size_mask() const { return _block_size_mask; }

	int get_voxel(Vector3i pos, unsigned int c = 0);
	void set_voxel(int value, Vector3i pos, unsigned int c = 0);

	float get_voxel_f(int x, int y, int z, unsigned int c = VoxelBuffer::CHANNEL_ISOLEVEL);
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int c = VoxelBuffer::CHANNEL_ISOLEVEL);

	void set_default_voxel(int value, unsigned int channel = 0);
	int get_default_voxel(unsigned int channel = 0);

	// Gets a copy of all voxels in the area starting at min_pos having the same size as dst_buffer.
	void get_buffer_copy(Vector3i min_pos, VoxelBuffer &dst_buffer, unsigned int channels_mask = 1);

	// Moves the given buffer into a block of the map. The buffer is referenced, no copy is made.
	void set_block_buffer(Vector3i bpos, Ref<VoxelBuffer> buffer);

	struct NoAction {
		inline void operator()(VoxelBlock *block) {}
	};

	template <typename Action_T>
	void remove_block(Vector3i bpos, Action_T pre_delete) {
		if (_last_accessed_block && _last_accessed_block->pos == bpos)
			_last_accessed_block = NULL;
		VoxelBlock **pptr = _blocks.getptr(bpos);
		if (pptr) {
			VoxelBlock *block = *pptr;
			ERR_FAIL_COND(block == NULL);
			pre_delete(block);
			memdelete(block);
			_blocks.erase(bpos);
		}
	}

	/*template <typename Action_T>
	void remove_blocks_not_in_area(Vector3i min, Vector3i max, Action_T pre_delete = NoAction()) {

		Vector3i::sort_min_max(min, max);

		Vector<Vector3i> to_remove;
		const Vector3i *key = NULL;

		while (key = _blocks.next(key)) {

			VoxelBlock *block_ptr = _blocks.get(*key);
			ERR_FAIL_COND(block_ptr == NULL); // Should never trigger

			if (block_ptr->pos.is_contained_in(min, max)) {

				to_remove.push_back(*key);

				if (block_ptr == _last_accessed_block)
					_last_accessed_block = NULL;

				pre_delete(block_ptr);
				memdelete(block_ptr);
			}
		}

		for (unsigned int i = 0; i < to_remove.size(); ++i) {
			_blocks.erase(to_remove[i]);
		}
	}*/

	VoxelBlock *get_block(Vector3i bpos);

	bool has_block(Vector3i pos) const;
	bool is_block_surrounded(Vector3i pos) const;

	void clear();

	template <typename Op_T>
	void for_all_blocks(Op_T op) {
		const Vector3i *key = NULL;
		while (key = _blocks.next(key)) {
			VoxelBlock *block = _blocks.get(*key);
			if (block != NULL) {
				op(block);
			}
		}
	}

private:
	void set_block(Vector3i bpos, VoxelBlock *block);
	VoxelBlock *get_or_create_block_at_voxel_pos(Vector3i pos);

	void set_block_size_pow2(unsigned int p);

	static void _bind_methods();

	_FORCE_INLINE_ int _get_voxel_binding(int x, int y, int z, unsigned int c = 0) { return get_voxel(Vector3i(x, y, z), c); }
	_FORCE_INLINE_ void _set_voxel_binding(int value, int x, int y, int z, unsigned int c = 0) { set_voxel(value, Vector3i(x, y, z), c); }
	_FORCE_INLINE_ int _get_voxel_v_binding(Vector3 pos, unsigned int c = 0) { return get_voxel(Vector3i(pos), c); }
	_FORCE_INLINE_ void _set_voxel_v_binding(int value, Vector3 pos, unsigned int c = 0) { set_voxel(value, Vector3i(pos), c); }
	_FORCE_INLINE_ bool _has_block_binding(int x, int y, int z) { return has_block(Vector3i(x, y, z)); }
	_FORCE_INLINE_ Vector3 _voxel_to_block_binding(Vector3 pos) const { return voxel_to_block(Vector3i(pos)).to_vec3(); }
	_FORCE_INLINE_ Vector3 _block_to_voxel_binding(Vector3 pos) const { return block_to_voxel(Vector3i(pos)).to_vec3(); }
	bool _is_block_surrounded(Vector3 pos) const { return is_block_surrounded(Vector3i(pos)); }
	void _get_buffer_copy_binding(Vector3 pos, Ref<VoxelBuffer> dst_buffer_ref, unsigned int channel = 0);
	void _set_block_buffer_binding(Vector3 bpos, Ref<VoxelBuffer> buffer) { set_block_buffer(Vector3i(bpos), buffer); }

private:
	// Voxel values that will be returned if access is out of map bounds
	uint8_t _default_voxel[VoxelBuffer::MAX_CHANNELS];

	// Blocks stored with a spatial hash in all 3D directions
	HashMap<Vector3i, VoxelBlock *, Vector3iHasher> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	VoxelBlock *_last_accessed_block;

	unsigned int _block_size;
	unsigned int _block_size_pow2;
	unsigned int _block_size_mask;
};

#endif // VOXEL_MAP_H
