#ifndef VOXEL_MODIFIER_SPHERE_H
#define VOXEL_MODIFIER_SPHERE_H

#include "voxel_modifier_sdf.h"

namespace zylann::voxel {

class VoxelModifierSphere : public VoxelModifierSdf {
public:
	Type get_type() const override {
		return TYPE_SPHERE;
	};
	void set_radius(real_t radius);
	real_t get_radius() const;
	void apply(VoxelModifierContext ctx) const override;
	void get_shader_data(ShaderData &out_shader_data) override;

protected:
	void update_aabb() override;

private:
	real_t _radius = 10.f;
};

} // namespace zylann::voxel

#endif // VOXEL_MODIFIER_SPHERE_H
