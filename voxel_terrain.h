#ifndef VOXEL_TERRAIN_H
#define VOXEL_TERRAIN_H

#include <scene/main/node.h>
#include "voxel_map.h"
#include "voxel_mesher.h"

// Infinite static terrain made of voxels.
// It is loaded around VoxelTerrainStreamers.
class VoxelTerrain : public Node /*, public IVoxelMapObserver*/ {
	OBJ_TYPE(VoxelTerrain, Node)

	// Parameters
	int _min_y; // In blocks, not voxels
	int _max_y;

	// Voxel storage
	Ref<VoxelMap> _map;

	Vector<Vector3i> _block_update_queue;
	Ref<VoxelMesher> _mesher;

public:
	VoxelTerrain();

	void force_load_blocks(Vector3i center, Vector3i extents);

	int get_block_update_count();

	Ref<VoxelMesher> get_mesher() { return _mesher; }

protected:
	void _notification(int p_what);
	void _process();

	void update_blocks();
	void update_block_mesh(Vector3i block_pos);

	// Observer events
	//void block_removed(VoxelBlock & block);

	static void _bind_methods();

	// Convenience
	Vector3 _voxel_to_block_binding(Vector3 pos) { return Vector3i(VoxelMap::voxel_to_block(pos)).to_vec3(); }
	Vector3 _block_to_voxel_binding(Vector3 pos) { return Vector3i(VoxelMap::block_to_voxel(pos)).to_vec3(); }
	void _force_load_blocks_binding(Vector3 center, Vector3 extents) { force_load_blocks(center, extents); }

};

#endif // VOXEL_TERRAIN_H

