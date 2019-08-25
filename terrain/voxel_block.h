#ifndef VOXEL_BLOCK_H
#define VOXEL_BLOCK_H

#include "../util/direct_mesh_instance.h"
#include "../util/direct_static_body.h"
#include "../voxel_buffer.h"

class Spatial;

// Internal structure holding a reference to mesh visuals, physics and a block of voxel data.
class VoxelBlock {
public:
	enum MeshState {
		MESH_NEVER_UPDATED = 0,
		MESH_UP_TO_DATE,
		MESH_UPDATE_NOT_SENT, // The mesh is out of date, but no update request have been sent
		MESH_UPDATE_SENT // The mesh is out of date, and an update request is pending
	};

	Ref<VoxelBuffer> voxels;
	Vector3i position;
	unsigned int lod_index = 0;
	bool modified = false; // Indicates if this block should be saved

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index);

	~VoxelBlock();

	void set_mesh(Ref<Mesh> mesh, Spatial *node, bool generate_collision, bool debug_collision);
	bool has_mesh() const;

	void set_mesh_state(MeshState ms);
	MeshState get_mesh_state() const;

	void mark_been_meshed();
	bool has_been_meshed() const;

	void set_world(World *world);
	void set_visible(bool visible);
	bool is_visible() const;

	inline bool is_mesh_update_scheduled() {
		return _mesh_state == MESH_UPDATE_NOT_SENT || _mesh_state == MESH_UPDATE_SENT;
	}

private:
	VoxelBlock();

	Vector3i _position_in_voxels;

	DirectMeshInstance _mesh_instance;
	DirectStaticBody _static_body;

	int _mesh_update_count = 0;
	bool _visible = true;
	MeshState _mesh_state = MESH_NEVER_UPDATED;

	// The mesh might be null, but we don't know if it's actually empty or if it's loading.
	// This boolean tells if we attempted to mesh this block at least once.
	bool _has_been_meshed = false;
};

#endif // VOXEL_BLOCK_H
