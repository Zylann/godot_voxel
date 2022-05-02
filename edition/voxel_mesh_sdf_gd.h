#ifndef VOXEL_MESH_SDF_GD_H
#define VOXEL_MESH_SDF_GD_H

#include "../storage/voxel_buffer_gd.h"
#include "../util/math/vector3f.h"

#include <core/object/ref_counted.h>

class Mesh;
class SceneTree;

namespace zylann::voxel {

// Contains the baked signed distance field of a mesh, which can be used to sculpt terrain.
// TODO Make it a resource so we can pre-build, save and load the baked data more easily
class VoxelMeshSDF : public RefCounted {
	GDCLASS(VoxelMeshSDF, RefCounted)
public:
	enum BakeMode { //
		BAKE_MODE_ACCURATE_NAIVE,
		BAKE_MODE_ACCURATE_PARTITIONED,
		BAKE_MODE_APPROX_INTERP,
		//BAKE_MODE_APPROX_SWEEP,
		BAKE_MODE_COUNT
	};

	// The data cannot be used until baked
	bool is_baked() const;
	bool is_baking() const;

	int get_cell_count() const;
	void set_cell_count(int cc);

	float get_margin_ratio() const;
	void set_margin_ratio(float mr);

	BakeMode get_bake_mode() const;
	void set_bake_mode(BakeMode mode);

	int get_partition_subdiv() const;
	void set_partition_subdiv(int subdiv);

	void set_boundary_sign_fix_enabled(bool enable);
	bool is_boundary_sign_fix_enabled() const;

	// Note, the mesh is not referenced because we don't want it to be a dependency.
	// An SDF should be usable even without the original mesh.
	// The mesh is only necessary when baking.

	void bake(Ref<Mesh> mesh);

	// Bakes the SDF asynchronously using threads of the job system.
	// TODO A reference to the SceneTree should not be necessary!
	// It is currently needed to ensure `VoxelServerUpdater` gets created so it can tick the task system...
	void bake_async(Ref<Mesh> mesh, SceneTree *scene_tree);

	Ref<gd::VoxelBuffer> get_voxel_buffer() const;
	AABB get_aabb() const;

	Array debug_check_sdf(Ref<Mesh> mesh);

private:
	void _on_bake_async_completed(Ref<gd::VoxelBuffer> buffer, Vector3 min_pos, Vector3 max_pos);

	static void _bind_methods();

	// Data
	Ref<gd::VoxelBuffer> _voxel_buffer;
	Vector3f _min_pos;
	Vector3f _max_pos;

	// States
	bool _is_baking = false;

	// Baking options
	int _cell_count = 32;
	float _margin_ratio = 0.25;
	BakeMode _bake_mode = BAKE_MODE_ACCURATE_PARTITIONED;
	uint8_t _partition_subdiv = 32;
	bool _boundary_sign_fix = true;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMeshSDF::BakeMode);

#endif // VOXEL_MESH_SDF_GD_H
