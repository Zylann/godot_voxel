#ifndef VOXEL_MODIFIER_MESH_GD_H
#define VOXEL_MODIFIER_MESH_GD_H

#include "../../edition/voxel_mesh_sdf_gd.h"
#include "voxel_modifier_gd.h"

namespace zylann::voxel::gd {

class VoxelModifierMesh : public VoxelModifier {
	GDCLASS(VoxelModifierMesh, VoxelModifier);

public:
	void set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf);
	Ref<VoxelMeshSDF> get_mesh_sdf() const;

	void set_isolevel(float isolevel);
	float get_isolevel() const;

protected:
	zylann::voxel::VoxelModifier *create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) override;

private:
	void _on_mesh_sdf_baked();
	static void _bind_methods();

	Ref<VoxelMeshSDF> _mesh_sdf;
	float _isolevel = 0.0f;
};

} // namespace zylann::voxel::gd

#endif // VOXEL_MODIFIER_MESH_GD_H
