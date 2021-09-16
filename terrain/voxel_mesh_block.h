#ifndef VOXEL_MESH_BLOCK_H
#define VOXEL_MESH_BLOCK_H

#include "../constants/cube_tables.h"
#include "../storage/voxel_ref_count.h"
#include "../util/godot/direct_mesh_instance.h"
#include "../util/godot/direct_static_body.h"

class Spatial;

// Stores mesh and collider for one chunk of the rendered volume.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
class VoxelMeshBlock {
public:
	enum MeshState {
		MESH_NEVER_UPDATED = 0, // TODO Redundant with MESH_NEED_UPDATE?
		MESH_UP_TO_DATE,
		MESH_NEED_UPDATE, // The mesh is out of date but was not yet scheduled for update
		MESH_UPDATE_NOT_SENT, // The mesh is out of date and was scheduled for update, but no request have been sent yet
		MESH_UPDATE_SENT // The mesh is out of date, and an update request was sent, pending response
	};

	enum FadingState {
		FADING_NONE,
		FADING_IN,
		FADING_OUT
	};

	Vector3i position; // In blocks
	unsigned int lod_index = 0;
	bool pending_transition_update = false;
	VoxelRefCount mesh_viewers;
	VoxelRefCount collision_viewers;
	bool got_first_mesh_update = false;

	uint32_t last_collider_update_time = 0;
	bool has_deferred_collider_update = false;
	Vector<Array> deferred_collider_data;

	FadingState fading_state = FADING_NONE;
	float fading_progress = 0.f;
	// Voxel LOD works by splitting a block into up to 8 higher-resolution blocks.
	// The parent block and its children can be called a "LOD group".
	// Only non-overlapping blocks in a LOD group can be considered active at once.
	// So when LOD fading is used, we no longer use `visible` to find which block is active,
	// because blocks can use a cross-fade effect. Overlapping blocks of the same LOD group can be visible at once.
	// Hence the need to use this boolean.
	bool active = false;

	static VoxelMeshBlock *create(Vector3i bpos, unsigned int size, unsigned int p_lod_index);

	~VoxelMeshBlock();

	void set_world(Ref<World> p_world);

	// Visuals

	void set_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_mesh() const;
	void set_transition_mesh(Ref<Mesh> mesh, int side);
	bool has_mesh() const;
	void drop_mesh();

	void set_shader_material(Ref<ShaderMaterial> material);
	inline Ref<ShaderMaterial> get_shader_material() const { return _shader_material; }

	// Collisions

	void set_collision_mesh(Vector<Array> surface_arrays, bool debug_collision, Spatial *node, float margin);
	void set_collision_layer(int layer);
	void set_collision_mask(int mask);
	void set_collision_margin(float margin);
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

	bool update_fading(float speed);

private:
	VoxelMeshBlock();

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

	// Must match default value of `active`
	bool _visible = false;

	bool _parent_visible = true;
	MeshState _mesh_state = MESH_NEVER_UPDATED;
	uint8_t _transition_mask = 0;
};

#endif // VOXEL_MESH_BLOCK_H
