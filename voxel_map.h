#ifndef VOXEL_MAP_H
#define VOXEL_MAP_H

#include <scene/main/node.h>
#include <core/hash_map.h>
#include <scene/3d/mesh_instance.h>
#include "voxel_buffer.h"


// Fixed-size voxel container used in VoxelMap. Used internally.
class VoxelBlock {
public:
	static const int SIZE_POW2 = 4; // 3=>8, 4=>16, 5=>32...
	static const int SIZE = 1 << SIZE_POW2;

	Ref<VoxelBuffer> voxels; // SIZE*SIZE*SIZE voxels
	Vector3i pos;
	NodePath mesh_instance_path;

	static VoxelBlock * create(Vector3i bpos, VoxelBuffer * buffer = 0);

	MeshInstance * get_mesh_instance(const Node & root);

private:
	VoxelBlock();

};


// Infinite voxel storage by means of octants like Gridmap
class VoxelMap : public Reference {
	OBJ_TYPE(VoxelMap, Reference)
public:
	// Converts voxel coodinates into block coordinates
	static _FORCE_INLINE_ Vector3i voxel_to_block(Vector3i pos) {
		return Vector3i(
			pos.x >> VoxelBlock::SIZE_POW2,
			pos.y >> VoxelBlock::SIZE_POW2,
			pos.z >> VoxelBlock::SIZE_POW2
		);
	}

	// Converts block coodinates into voxel coordinates
	static _FORCE_INLINE_ Vector3i block_to_voxel(Vector3i bpos) {
		return bpos * VoxelBlock::SIZE;
	}

	VoxelMap();
	~VoxelMap();

	int get_voxel(Vector3i pos, unsigned int c = 0);
	void set_voxel(int value, Vector3i pos, unsigned int c = 0);

	void set_default_voxel(int value, unsigned int channel=0);
	int get_default_voxel(unsigned int channel=0);

	// Gets a copy of all voxels in the area starting at min_pos having the same size as dst_buffer.
	void get_buffer_copy(Vector3i min_pos, VoxelBuffer & dst_buffer, unsigned int channel = 0);

	// Moves the given buffer into a block of the map. The buffer is referenced, no copy is made.
	void set_block_buffer(Vector3i bpos, Ref<VoxelBuffer> buffer);

	void remove_blocks_not_in_area(Vector3i min, Vector3i max);

	VoxelBlock * get_block(Vector3i bpos);

	bool has_block(Vector3i pos) const;
	bool is_block_surrounded(Vector3i pos) const;

	void clear();

private:
	void set_block(Vector3i bpos, VoxelBlock * block);

	_FORCE_INLINE_ int get_block_size() const { return VoxelBlock::SIZE; }

	static void _bind_methods();

	_FORCE_INLINE_ int _get_voxel_binding(int x, int y, int z, unsigned int c = 0) { return get_voxel(Vector3i(x, y, z), c); }
	_FORCE_INLINE_ void _set_voxel_binding(int value, int x, int y, int z, unsigned int c = 0) { set_voxel(value, Vector3i(x, y, z), c); }
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
	HashMap<Vector3i, VoxelBlock*, Vector3iHasher> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	VoxelBlock * _last_accessed_block;

};

#endif // VOXEL_MAP_H

