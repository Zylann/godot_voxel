#ifndef VOXEL_MESH_SDF_GD_H
#define VOXEL_MESH_SDF_GD_H

#include "../engine/compute_shader_resource.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/classes/mesh.h"
#include "../util/godot/classes/resource.h"
#include "../util/math/vector3f.h"
#include "../util/thread/mutex.h"

ZN_GODOT_FORWARD_DECLARE(class SceneTree);

namespace zylann::voxel {

// Contains the baked signed distance field of a mesh, which can be used to sculpt terrain.
class VoxelMeshSDF : public Resource {
	GDCLASS(VoxelMeshSDF, Resource)
public:
	enum BakeMode { //
		BAKE_MODE_ACCURATE_NAIVE,
		BAKE_MODE_ACCURATE_PARTITIONED,
		BAKE_MODE_APPROX_INTERP,
		BAKE_MODE_APPROX_FLOODFILL,
		BAKE_MODE_COUNT
	};

	static const int MIN_CELL_COUNT = 2;
	static const int MAX_CELL_COUNT = 256;

	static constexpr float MIN_MARGIN_RATIO = 0.f;
	static constexpr float MAX_MARGIN_RATIO = 1.f;

	static const int MIN_PARTITION_SUBDIV = 2;
	static const int MAX_PARTITION_SUBDIV = 255;

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

	void set_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_mesh() const;

	void bake();

// Bakes the SDF asynchronously using threads of the job system.
// TODO A reference to the SceneTree should not be necessary!
// It is currently needed to ensure `VoxelServerUpdater` gets created so it can tick the task system...
// TODO GDX: it seems binding a method taking a `SceneTree*` fails to compile. It is supposed to be working.
#ifdef ZN_GODOT_EXTENSION
	void bake_async(Object *scene_tree_o);
#else
	void bake_async(SceneTree *scene_tree);
#endif

	// Accesses baked SDF data.
	// WARNING: don't modify this buffer. Only read from it.
	// There are some usages (like modifiers) that will read it from different threads,
	// but there is no thread safety in case of direct modification.
	// TODO Introduce a VoxelBufferReadOnly? Since that's likely the only way in an object-oriented script API...
	Ref<gd::VoxelBuffer> get_voxel_buffer() const;

	// Gets the padded bounding box of the model. This is important to know for signed distances to be coherent.
	AABB get_aabb() const;

	inline Vector3f get_aabb_min_pos() const {
		return _min_pos;
	}
	inline Vector3f get_aabb_max_pos() const {
		return _max_pos;
	}

	std::shared_ptr<ComputeShaderResource> get_gpu_resource();

	Array debug_check_sdf(Ref<Mesh> mesh);

private:
	void _on_bake_async_completed(Ref<gd::VoxelBuffer> buffer, Vector3 min_pos, Vector3 max_pos);

	Dictionary _b_get_data() const;
	void _b_set_data(Dictionary d);

	static void _bind_methods();

	// Data
	Ref<gd::VoxelBuffer> _voxel_buffer;
	Vector3f _min_pos;
	Vector3f _max_pos;
	// Stored as a shared_ptr in case that resource is in use while being re-generated
	std::shared_ptr<ComputeShaderResource> _gpu_resource;
	Mutex _gpu_resource_mutex;

	// States
	bool _is_baking = false;

	// Baking options
	int _cell_count = 64;
	float _margin_ratio = 0.25;
	BakeMode _bake_mode = BAKE_MODE_ACCURATE_PARTITIONED;
	uint8_t _partition_subdiv = 32;
	bool _boundary_sign_fix = true;
	// Note, the mesh is referenced here only for convenience. Setting it to null will not clear the SDF.
	// An SDF should be usable without loading the original mesh onto the graphics card.
	// The mesh is only used for baking.
	Ref<Mesh> _mesh;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMeshSDF::BakeMode);

#endif // VOXEL_MESH_SDF_GD_H
