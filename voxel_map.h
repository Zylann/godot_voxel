#ifndef VOXEL_MAP_H
#define VOXEL_MAP_H

#include <scene/main/node.h>
#include <core/hash_map.h>
#include <scene/3d/mesh_instance.h>
#include "voxel_buffer.h"

class VoxelMap;

// Fixed-size voxel container used in VoxelMap. Used internally.
class VoxelBlock : public Reference {
	OBJ_TYPE(VoxelBlock, Reference)
public:
	static const int SIZE_POW2 = 4; // 3=>8, 4=>16, 5=>32...
	static const int SIZE = 1 << SIZE_POW2;

	Ref<VoxelBuffer> voxels; // SIZE*SIZE*SIZE voxels
	Vector3i pos;
	NodePath mesh_instance_path;
	//VoxelMap * map;

	MeshInstance * get_mesh_instance(const Node & root);

	static VoxelBlock * create(Vector3i bpos, VoxelBuffer * buffer = 0);

	~VoxelBlock();

private:
	VoxelBlock() : Reference(), voxels(NULL) {}

};

//class IVoxelMapObserver {
//public:
//    virtual void block_removed(VoxelBlock & block) = 0;
//};

// Infinite voxel storage by means of octants like Gridmap
class VoxelMap : public Reference {
	OBJ_TYPE(VoxelMap, Reference)
public:
	VoxelMap();
	~VoxelMap();

	int get_voxel(Vector3i pos, unsigned int c = 0);
	void set_voxel(int value, Vector3i pos, unsigned int c = 0);

	void set_default_voxel(int value, unsigned int channel=0);
	int get_default_voxel(unsigned int channel=0);

	// Converts voxel coodinates into block coordinates
	static _FORCE_INLINE_ Vector3i voxel_to_block(Vector3i pos) {
		return Vector3i(
			//pos.x < 0 ? (pos.x + 1) / VoxelBlock::SIZE - 1 : pos.x / VoxelBlock::SIZE,
			//pos.y < 0 ? (pos.y + 1) / VoxelBlock::SIZE - 1 : pos.y / VoxelBlock::SIZE,
			//pos.z < 0 ? (pos.z + 1) / VoxelBlock::SIZE - 1 : pos.z / VoxelBlock::SIZE
			pos.x >> VoxelBlock::SIZE_POW2,
			pos.y >> VoxelBlock::SIZE_POW2,
			pos.z >> VoxelBlock::SIZE_POW2
		);
	}

	// Converts block coodinates into voxel coordinates
	static _FORCE_INLINE_ Vector3i block_to_voxel(Vector3i bpos) {
		return bpos * VoxelBlock::SIZE;
	}

	// Gets a copy of all voxels in the area starting at min_pos having the same size as dst_buffer.
	void get_buffer_copy(Vector3i min_pos, VoxelBuffer & dst_buffer, unsigned int channel = 0);

	// Moves the given buffer into a block of the map. The buffer is referenced, no copy is made.
	void set_block_buffer(Vector3i bpos, Ref<VoxelBuffer> buffer);

	void remove_blocks_not_in_area(Vector3i min, Vector3i max);

	_FORCE_INLINE_ Ref<VoxelBlock> get_block_ref(Vector3i bpos) { return get_block(bpos); }

	bool has_block(Vector3i pos) const;
	bool is_block_surrounded(Vector3i pos) const;

	//void set_observer(IVoxelMapObserver * observer) { _observer = observer; }

private:
	VoxelBlock * get_block(Vector3i bpos);
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
	HashMap<Vector3i, Ref<VoxelBlock>, Vector3iHasher> _blocks;

	// Voxel access will most frequently be in contiguous areas, so the same blocks are accessed.
	// To prevent too much hashing, this reference is checked before.
	VoxelBlock * _last_accessed_block;

	//IVoxelMapObserver * _observer;
};

//class VoxelSector {
//public:
//    static const int SIZE = 16;
//
//private:
//    Ref<VoxelBlock> blocks[SIZE * SIZE * SIZE];
//};

//template <unsigned int P>
//struct VoxelTree {
//
//    const int SIZE = 1 << P;
//    const int VOLUME = P*P*P;
//
//    uint8_t get_voxel(int x, int y, int z) {
//        unsigned int i = index(x / SIZE, y / SIZE, z / SIZE);
//        ERR_FAIL_COND_V(i >= VOLUME, 0);
//        if (subtrees) {
//            VoxelTree<N> * subtree = subtrees[i];
//            if (subtree) {
//                return subtree->get_voxel(x, y, z);
//            }
//        }
//        else if (blocks[i].is_valid()) {
//            return blocks[i]->voxels.get_voxel(x, y, z);
//        }
//        return 0;
//    }
//
//    unsigned int index(int x, int y, int z) {
//        return (pos.z * SIZE + pos.x) * SIZE + pos.y;
//    }
//
//    Ref<VoxelBlock> * blocks;
//    VoxelTree<N> * subtrees;
//    int level;
//};

#endif // VOXEL_MAP_H

