#ifndef VOXEL_MESH_BLOCK_VLT_H
#define VOXEL_MESH_BLOCK_VLT_H

#include "../voxel_mesh_block.h"

namespace zylann::voxel {

// Stores mesh and collider for one chunk of `VoxelTerrain`.
// It doesn't store voxel data, because it may be using different block size, or different data structure.
class VoxelMeshBlockVLT : public VoxelMeshBlock {
public:
	enum FadingState { //
		FADING_NONE,
		FADING_IN,
		FADING_OUT
	};

	uint8_t lod_index = 0;

	FadingState fading_state = FADING_NONE;
	float fading_progress = 0.f;
	// Voxel LOD works by splitting a block into up to 8 higher-resolution blocks.
	// The parent block and its children can be called a "LOD group".
	// Only non-overlapping blocks in a LOD group can be considered active at once.
	// So when LOD fading is used, we no longer use `visible` to find which block is active,
	// because blocks can use a cross-fade effect. Overlapping blocks of the same LOD group can be visible at once.
	// Hence the need to use this boolean.
	bool active = false;

	bool got_first_mesh_update = false;

	uint32_t last_collider_update_time = 0;
	bool has_deferred_collider_update = false;
	std::vector<Array> deferred_collider_data;

	VoxelMeshBlockVLT(const Vector3i bpos, unsigned int size, unsigned int p_lod_index);
	~VoxelMeshBlockVLT();

	void set_world(Ref<World3D> p_world);
	void set_visible(bool visible);
	bool update_fading(float speed);

	void set_parent_visible(bool parent_visible);

	void set_mesh(Ref<Mesh> mesh, DirectMeshInstance::GIMode gi_mode);

	void set_transition_mask(uint8_t m);
	inline uint8_t get_transition_mask() const {
		return _transition_mask;
	}

	void set_gi_mode(DirectMeshInstance::GIMode mode);
	void set_transition_mesh(Ref<Mesh> mesh, int side, DirectMeshInstance::GIMode gi_mode);
	void set_shader_material(Ref<ShaderMaterial> material);
	inline Ref<ShaderMaterial> get_shader_material() const {
		return _shader_material;
	}

	void set_parent_transform(const Transform3D &parent_transform);

	template <typename F>
	void for_each_mesh_instance_with_transform(F f) const {
		const Transform3D local_transform(Basis(), _position_in_voxels);
		const Transform3D world_transform = local_transform;
		f(_mesh_instance, world_transform);
		for (unsigned int i = 0; i < _transition_mesh_instances.size(); ++i) {
			const DirectMeshInstance &mi = _transition_mesh_instances[i];
			if (mi.is_valid()) {
				f(mi, world_transform);
			}
		}
	}

private:
	void _set_visible(bool visible);

	inline bool _is_transition_visible(int side) const {
		return _transition_mask & (1 << side);
	}

	Ref<ShaderMaterial> _shader_material;

	FixedArray<DirectMeshInstance, Cube::SIDE_COUNT> _transition_mesh_instances;

	uint8_t _transition_mask = 0;

#ifdef VOXEL_DEBUG_LOD_MATERIALS
	Ref<Material> _debug_material;
	Ref<Material> _debug_transition_material;
#endif
};

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_VLT_H
