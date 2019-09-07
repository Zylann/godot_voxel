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
		MESH_NEVER_UPDATED = 0, // TODO Redundant with MESH_NEED_UPDATE?
		MESH_UP_TO_DATE,
		MESH_NEED_UPDATE, // The mesh is out of date but was not yet scheduled for update
		MESH_UPDATE_NOT_SENT, // The mesh is out of date and was scheduled for update, but no request have been sent yet
		MESH_UPDATE_SENT // The mesh is out of date, and an update request was sent, pending response
	};

	Ref<VoxelBuffer> voxels;
	Vector3i position;
	unsigned int lod_index = 0;

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index);

	~VoxelBlock();

	void set_mesh(Ref<Mesh> mesh, Spatial *node, bool generate_collision, Array surface_arrays, bool debug_collision);
	bool has_mesh() const;

	void set_mesh_state(MeshState ms);
	MeshState get_mesh_state() const;

	void set_needs_lodding(bool need_lodding);
	inline bool get_needs_lodding() const { return _needs_lodding; }

	void set_world(World *world);

	void set_visible(bool visible);
	bool is_visible() const;

	void set_parent_visible(bool parent_visible);

	bool is_modified() const;
	void set_modified(bool modified);

private:
	VoxelBlock();

	void _set_visible(bool visible);

private:
	Vector3i _position_in_voxels;

	DirectMeshInstance _mesh_instance;
	DirectStaticBody _static_body;

	int _mesh_update_count = 0;
	bool _visible = true;
	bool _parent_visible = true;
	MeshState _mesh_state = MESH_NEVER_UPDATED;

	// The block was edited, which requires its LOD counterparts to be recomputed
	bool _needs_lodding = false;

	// Indicates if this block is different from the time it was loaded (should be saved)
	bool _modified = false;
};

#endif // VOXEL_BLOCK_H
