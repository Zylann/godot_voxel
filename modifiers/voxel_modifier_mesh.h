#ifndef VOXEL_MODIFIER_MESH_H
#define VOXEL_MODIFIER_MESH_H

#include "../edition/voxel_mesh_sdf_gd.h"
#include "voxel_modifier_sdf.h"

namespace zylann::voxel {

class VoxelModifierMesh : public VoxelModifierSdf {
public:
	Type get_type() const override {
		return TYPE_MESH;
	};

	void set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf);
	void set_isolevel(float isolevel);
	void apply(VoxelModifierContext ctx) const override;
	void get_shader_data(ShaderData &out_shader_data) override;
	void request_shader_data_update();

protected:
	void update_aabb() override;

private:
	// Originally I wanted to keep the core of modifiers separate from Godot stuff, but in order to also support
	// GPU resources, putting this here was easier.
	Ref<VoxelMeshSDF> _mesh_sdf;
	float _isolevel;
};

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_MESH_H
