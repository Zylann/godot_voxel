#include "voxel_modifier_sphere_gd.h"
#include "../voxel_modifier_sphere.h"

namespace zylann::voxel::gd {

zylann::voxel::VoxelModifierSphere *get_sphere(VoxelLodTerrain &volume, uint32_t id) {
	return get_modifier<zylann::voxel::VoxelModifierSphere>(volume, id, zylann::voxel::VoxelModifier::TYPE_SPHERE);
}

float VoxelModifierSphere::get_radius() const {
	return _radius;
}

void VoxelModifierSphere::set_radius(float r) {
	_radius = math::max(r, 0.f);
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierSphere *sphere = get_sphere(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(sphere != nullptr);
	const AABB prev_aabb = sphere->get_aabb();
	sphere->set_radius(r);
	const AABB new_aabb = sphere->get_aabb();
	post_edit_modifier(*_volume, prev_aabb);
	post_edit_modifier(*_volume, new_aabb);
}

zylann::voxel::VoxelModifier *VoxelModifierSphere::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	zylann::voxel::VoxelModifierSphere *sphere = modifiers.add_modifier<zylann::voxel::VoxelModifierSphere>(id);
	sphere->set_radius(_radius);
	return sphere;
}

void VoxelModifierSphere::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "r"), &VoxelModifierSphere::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &VoxelModifierSphere::get_radius);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.0, 100.0, 0.1"), "set_radius", "get_radius");
}

} // namespace zylann::voxel::gd
