#ifndef VOXEL_MODIFIER_SPHERE_H
#define VOXEL_MODIFIER_SPHERE_H

#include "voxel_modifier_sdf.h"

namespace zylann::voxel {

class VoxelModifierSphere : public VoxelModifierSdf {
public:
	Type get_type() const override {
		return TYPE_SPHERE;
	};
	void set_radius(float radius);
	float get_radius() const;
	void apply(VoxelModifierContext ctx) const override;

#ifdef VOXEL_ENABLE_GPU
	void get_shader_data(ShaderData &out_shader_data) override;
#endif

protected:
	void update_aabb() override;

private:
	float _radius = 10.f;
};

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_SPHERE_H
