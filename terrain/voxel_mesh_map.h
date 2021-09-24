#ifndef VOXEL_MESH_MAP_H
#define VOXEL_MESH_MAP_H

#include "voxel_mesh_block.h"
#include <vector>

// Stores meshes and colliders in an infinite sparse grid of chunks (aka blocks).
class VoxelMeshMap {
public:
	// Converts voxel coodinates into block coordinates.
	// Don't use division because it introduces an offset in negative coordinates.
	static inline Vector3i voxel_to_block_b(Vector3i pos, int block_size_pow2) {
		return pos >> block_size_pow2;
	}

	inline Vector3i voxel_to_block(Vector3i pos) const {
		return voxel_to_block_b(pos, _block_size_pow2);
	}

	inline Vector3i to_local(Vector3i pos) const {
		return Vector3i(
				pos.x & _block_size_mask,
				pos.y & _block_size_mask,
				pos.z & _block_size_mask);
	}

	// Converts block coodinates into voxel coordinates
	inline Vector3i block_to_voxel(Vector3i bpos) const {
		return bpos * _block_size;
	}

	VoxelMeshMap();
	~VoxelMeshMap();

	void create(unsigned int block_size_po2, int lod_index);

	inline unsigned int get_block_size() const { return _block_size; }
	inline unsigned int get_block_size_pow2() const { return _block_size_pow2; }
	inline unsigned int get_block_size_mask() const { return _block_size_mask; }

	void set_lod_index(int lod_index);
	unsigned int get_lod_index() const;

	struct NoAction {
		inline void operator()(VoxelMeshBlock *block) {}
	};

	template <typename Action_T>
	void remove_block(Vector3i bpos, Action_T pre_delete) {
		if (_last_accessed_block && _last_accessed_block->position == bpos) {
			_last_accessed_block = nullptr;
		}
		unsigned int *iptr = _blocks_map.getptr(bpos);
		if (iptr != nullptr) {
			const unsigned int i = *iptr;
#ifdef DEBUG_ENABLED
			CRASH_COND(i >= _blocks.size());
#endif
			VoxelMeshBlock *block = _blocks[i];
			ERR_FAIL_COND(block == nullptr);
			pre_delete(block);
			queue_free_mesh_block(block);
			remove_block_internal(bpos, i);
		}
	}

	VoxelMeshBlock *get_block(Vector3i bpos);
	const VoxelMeshBlock *get_block(Vector3i bpos) const;

	void set_block(Vector3i bpos, VoxelMeshBlock *block);

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

private:
	//VoxelMeshBlock *get_or_create_block_at_voxel_pos(Vector3i pos);
	VoxelMeshBlock *create_default_block(Vector3i bpos);
	void remove_block_internal(Vector3i bpos, unsigned int index);
	void queue_free_mesh_block(VoxelMeshBlock *block);

	void set_block_size_pow2(unsigned int p);

private:
	// Blocks stored with a spatial hash in all 3D directions.
	// RELATIONSHIP = 2 because it delivers better performance with this kind of key and hash (less collisions).
	HashMap<Vector3i, unsigned int, Vector3iHasher, HashMapComparatorDefault<Vector3i>, 3, 2> _blocks_map;
	// Blocks are stored in a vector to allow faster iteration over all of them
	std::vector<VoxelMeshBlock *> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	mutable VoxelMeshBlock *_last_accessed_block;

	unsigned int _block_size;
	unsigned int _block_size_pow2;
	unsigned int _block_size_mask;

	unsigned int _lod_index = 0;
};

#endif // VOXEL_MESH_BLOCK_MAP_H
