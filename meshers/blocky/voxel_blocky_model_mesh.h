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

	void bake(BakedData &baked_data, bool bake_tangents, MaterialIndexer &materials) override;

	void set_mesh_ortho_rotation_index(int i);
	int get_mesh_ortho_rotation_index() const;

	Ref<Mesh> get_preview_mesh() const override;

	void rotate_90(Vector3i::Axis axis, bool clockwise) override;

private:
	static void _bind_methods();

	Ref<Mesh> _mesh;
	uint8_t _mesh_ortho_rotation = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_BLOCKY_MODEL_MESH_H
