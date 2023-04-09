#ifndef VOXEL_MESH_BLOCK_VLT_H
#define VOXEL_MESH_BLOCK_VLT_H

#include "../../util/godot/classes/shader_material.h"
#include "../../util/memory.h"
#include "../../util/tasks/time_spread_task_runner.h"
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

	// 0 means not using fallback.
	// 1 means using texture of parent LOD (lod_index+1).
	// 2 means using texture of grand-parent LOD (lod_index+2), etc.
	uint8_t virtual_texture_fallback_level = 0;

	uint64_t last_collider_update_time = 0;
	UniquePtr<VoxelMesher::Output> deferred_collider_data;

	VoxelMeshBlockVLT(const Vector3i bpos, unsigned int size, unsigned int p_lod_index);
	~VoxelMeshBlockVLT();

	void set_world(Ref<World3D> p_world);
	void set_visible(bool visible);
	bool update_fading(float speed);

	void set_parent_visible(bool parent_visible);

	void set_mesh(
			Ref<Mesh> mesh, DirectMeshInstance::GIMode gi_mode, RenderingServer::ShadowCastingSetting shadow_casting);

	void set_transition_mask(uint8_t m);
	inline uint8_t get_transition_mask() const {
		return _transition_mask;
	}

	void set_gi_mode(DirectMeshInstance::GIMode mode);
	void set_shadow_casting(RenderingServer::ShadowCastingSetting mode);

	void set_transition_mesh(Ref<Mesh> mesh, unsigned int side, DirectMeshInstance::GIMode gi_mode,
			RenderingServer::ShadowCastingSetting shadow_casting);

	void set_shader_material(Ref<ShaderMaterial> material);
	inline Ref<ShaderMaterial> get_shader_material() const {
		return _shader_material;
	}

	void set_parent_transform(const Transform3D &parent_transform);
	void update_transition_mesh_transform(unsigned int side, const Transform3D &parent_transform);

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

	inline bool _is_transition_visible(unsigned int side) const {
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

Ref<ArrayMesh> build_mesh(Span<const VoxelMesher::Output::Surface> surfaces, Mesh::PrimitiveType primitive, int flags,
		Ref<Material> material);

} // namespace zylann::voxel

#endif // VOXEL_MESH_BLOCK_VLT_H
