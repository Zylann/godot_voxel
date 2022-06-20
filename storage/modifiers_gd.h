#ifndef VOXEL_MODIFIERS_GD_H
#define VOXEL_MODIFIERS_GD_H

#include "../edition/voxel_mesh_sdf_gd.h"
#include <scene/3d/node_3d.h>

namespace zylann::voxel {

class VoxelLodTerrain;
class VoxelModifier;
class VoxelModifierStack;

namespace gd {

class VoxelModifier : public Node3D {
	GDCLASS(VoxelModifier, Node3D)
public:
	enum Operation { //
		OPERATION_ADD,
		OPERATION_REMOVE,
		OPERATION_COUNT
	};

	VoxelModifier();

	void set_operation(Operation op);
	Operation get_operation() const;

	void set_smoothness(float s);
	float get_smoothness() const;

protected:
	virtual zylann::voxel::VoxelModifier *create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id);
	void _notification(int p_what);

	VoxelLodTerrain *_volume = nullptr;
	uint32_t _modifier_id = 0;

private:
	static void _bind_methods();

	Operation _operation = OPERATION_ADD;
	float _smoothness = 0.f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VoxelModifierSphere : public VoxelModifier {
	GDCLASS(VoxelModifierSphere, VoxelModifier);

public:
	float get_radius() const;
	void set_radius(float r);

protected:
	zylann::voxel::VoxelModifier *create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) override;

private:
	static void _bind_methods();

	float _radius = 10.f;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

} // namespace gd
} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::gd::VoxelModifier::Operation);

#endif // VOXEL_MODIFIERS_GD_H
