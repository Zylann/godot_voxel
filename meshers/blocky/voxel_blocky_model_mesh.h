#ifndef VOXEL_BLOCKY_MODEL_MESH_H
#define VOXEL_BLOCKY_MODEL_MESH_H

#include "voxel_blocky_model.h"

namespace zylann::voxel {

// Model using a mesh for visuals
class VoxelBlockyModelMesh : public VoxelBlockyModel {
	GDCLASS(VoxelBlockyModelMesh, VoxelBlockyModel)
public:
	void set_mesh(Ref<Mesh> mesh);
	Ref<Mesh> get_mesh() const {
		return _mesh;
	}

	void bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) const override;
	bool is_empty() const override;

	Ref<Mesh> get_preview_mesh() const override;

	void set_side_vertex_tolerance(float tolerance);
	float get_side_vertex_tolerance() const;

private:
	static void _bind_methods();

	Ref<Mesh> _mesh;
	// Margin near sides of the voxel where triangles will be considered to be "on the side". Those triangles will
	// be processed by the neighbor side culling system.
	float _side_vertex_tolerance = 0.001f;
};

void rotate_mesh_arrays(Span<Vector3f> vertices, Span<Vector3f> normals, Span<float> tangents, const Basis3f &basis);

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_MESH_H
