#include "voxel_blocky_fluid.h"
#include "voxel_blocky_model_cube.h"

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

namespace {

void bake_fluid(
		const VoxelBlockyFluid &fluid,
		VoxelBlockyFluid::BakedData &baked_fluid,
		VoxelBlockyModel::MaterialIndexer &materials
) {
	// for (const uint16_t model_index : baked_fluid.level_model_indices) {
	// 	if (model_index == VoxelBlockyModel::AIR_ID) {
	// 		ZN_PRINT_ERROR("Fluid is missing some levels");
	// 		break;
	// 	}
	// }

	// if (baked_fluid.level_model_indices.size() == 1) {
	// 	ZN_PRINT_ERROR("Fluid with only one level will not work properly");
	// }

	Ref<Material> material = fluid.get_material();
	baked_fluid.material_id = materials.get_or_create_index(material);

	// TODO This part shouldn't be necessary? it's the same for every fluid
	for (unsigned int side_index = 0; side_index < Cube::SIDE_COUNT; ++side_index) {
		make_cube_side_vertices_tangents(
				baked_fluid.side_surfaces[side_index], side_index, VoxelBlockyFluid::BakedData::TOP_HEIGHT, false
		);
	}

	for (unsigned int side_index = 0; side_index < Cube::SIDE_COUNT; ++side_index) {
		VoxelBlockyModel::BakedData::SideSurface &side_surface = baked_fluid.side_surfaces[side_index];

		// TODO Bake UVs with animation info somehow
		side_surface.uvs.resize(4);
		for (unsigned int i = 0; i < side_surface.uvs.size(); ++i) {
			side_surface.uvs[i] = Vector2f();
		}
	}
}

} // namespace

void VoxelBlockyFluid::bake(VoxelBlockyFluid::BakedData &baked_fluid, VoxelBlockyModel::MaterialIndexer &materials)
		const {
	bake_fluid(*this, baked_fluid, materials);
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
