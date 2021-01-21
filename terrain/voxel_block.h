#ifndef VOXEL_BLOCK_H
#define VOXEL_BLOCK_H

#include "../cube_tables.h"
#include "../storage/voxel_buffer.h"
#include "../util/direct_mesh_instance.h"
#include "../util/direct_static_body.h"
#include "../util/fixed_array.h"
#include "voxel_viewer_ref_count.h"

//#define VOXEL_DEBUG_LOD_MATERIALS

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
	bool pending_transition_update = false;
	VoxelViewerRefCount viewers;

	static VoxelBlock *create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size, unsigned int p_lod_index);

	~VoxelBlock();

	void set_world(Ref<World> p_world);

	// Visuals

	void set_mesh(Ref<Mesh> mesh);
	void set_transition_mesh(Ref<Mesh> mesh, int side);
	bool has_mesh() const;
	void drop_mesh();

	void set_shader_material(Ref<ShaderMaterial> material);
	inline Ref<ShaderMaterial> get_shader_material() const { return _shader_material; }

	// Collisions

	void set_collision_mesh(Vector<Array> surface_arrays, bool debug_collision, Spatial *node);
	void drop_collision();
	// TODO Collision layer and mask

	// State

	void set_mesh_state(MeshState ms);
	MeshState get_mesh_state() const;

	void set_visible(bool visible);
	bool is_visible() const;

	void set_parent_visible(bool parent_visible);
	void set_parent_transform(const Transform &parent_transform);

	void set_transition_mask(uint8_t m);
	//void set_transition_bit(uint8_t side, bool value);
	inline uint8_t get_transition_mask() const { return _transition_mask; }

	void set_needs_lodding(bool need_lodding);
	inline bool get_needs_lodding() const { return _needs_lodding; }

	bool is_modified() const;
	void set_modified(bool modified);

	template <typename F>
	void for_each_mesh_instance_with_transform(F f) const {
		const Transform local_transform(Basis(), _position_in_voxels.to_vec3());
		const Transform world_transform = local_transform;
		f(_mesh_instance, world_transform);
		for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
			const DirectMeshInstance &mi = _transition_mesh_instances[i];
			if (mi.is_valid()) {
				f(mi, world_transform);
			}
		}
	}

private:
	VoxelBlock();

	void _set_visible(bool visible);
	inline bool _is_transition_visible(int side) const { return _transition_mask & (1 << side); }

	inline void set_mesh_instance_visible(DirectMeshInstance &mi, bool visible) {
		if (visible) {
			mi.set_world(*_world);
		} else {
			mi.set_world(nullptr);
		}
	}

private:
	Vector3i _position_in_voxels;

	Ref<ShaderMaterial> _shader_material;
	DirectMeshInstance _mesh_instance;
	FixedArray<DirectMeshInstance, Cube::SIDE_COUNT> _transition_mesh_instances;
	DirectStaticBody _static_body;
	Ref<World> _world;

#ifdef VOXEL_DEBUG_LOD_MATERIALS
	Ref<Material> _debug_material;
	Ref<Material> _debug_transition_material;
#endif

	bool _visible = true;
	bool _parent_visible = true;
	MeshState _mesh_state = MESH_NEVER_UPDATED;
	uint8_t _transition_mask = 0;

	// The block was edited, which requires its LOD counterparts to be recomputed
	bool _needs_lodding = false;

	// Indicates if this block is different from the time it was loaded (should be saved)
	bool _modified = false;
};

#endif // VOXEL_BLOCK_H
