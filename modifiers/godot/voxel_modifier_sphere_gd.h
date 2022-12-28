#ifndef VOXEL_MODIFIER_SPHERE_GD_H
#define VOXEL_MODIFIER_SPHERE_GD_H

#include "voxel_modifier_gd.h"

namespace zylann::voxel::gd {

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

} // namespace zylann::voxel::gd

#endif // VOXEL_MODIFIER_SPHERE_GD_H
