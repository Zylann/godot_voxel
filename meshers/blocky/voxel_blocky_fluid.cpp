#include "voxel_blocky_fluid.h"

namespace zylann::voxel {

VoxelBlockyFluid::VoxelBlockyFluid() {}

void VoxelBlockyFluid::set_flowing_tile_strip(Rect2i tile_rect) {
	_flowing_tile_strip = tile_rect;
}

Rect2i VoxelBlockyFluid::get_flowing_tile_strip() const {
	return _flowing_tile_strip;
}

void VoxelBlockyFluid::set_idle_tile_strip(Rect2i tile_rect) {
	_idle_tile_strip = tile_rect;
}

Rect2i VoxelBlockyFluid::get_idle_tile_strip() const {
	return _idle_tile_strip;
}

void VoxelBlockyFluid::set_material(Ref<Material> material) {
	_material = material;
}

Ref<Material> VoxelBlockyFluid::get_material() const {
	return _material;
}

void VoxelBlockyFluid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_flowing_tile_strip", "tile_rect"), &VoxelBlockyFluid::set_flowing_tile_strip);
	ClassDB::bind_method(D_METHOD("get_flowing_tile_strip"), &VoxelBlockyFluid::get_flowing_tile_strip);

	ClassDB::bind_method(D_METHOD("set_idle_tile_strip", "tile_rect"), &VoxelBlockyFluid::set_idle_tile_strip);
	ClassDB::bind_method(D_METHOD("get_idle_tile_strip"), &VoxelBlockyFluid::get_idle_tile_strip);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &VoxelBlockyFluid::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VoxelBlockyFluid::get_material);

	ADD_PROPERTY(
			PropertyInfo(Variant::RECT2I, "flowing_tile_strip"), "set_flowing_tile_strip", "get_flowing_tile_strip"
	);

	ADD_PROPERTY(PropertyInfo(Variant::RECT2I, "idle_tile_strip"), "set_idle_tile_strip", "get_idle_tile_strip");

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"material",
					PROPERTY_HINT_RESOURCE_TYPE,
					zylann::godot::MATERIAL_3D_PROPERTY_HINT_STRING
			),
			"set_idle_tile_strip",
			"get_idle_tile_strip"
	);
}

} // namespace zylann::voxel
