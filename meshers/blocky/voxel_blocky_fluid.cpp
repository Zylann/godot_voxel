#include "voxel_blocky_fluid.h"
#include "../../util/godot/classes/material.h"
#include "blocky_material_indexer.h"
#include "voxel_blocky_model_cube.h"

namespace zylann::voxel {

VoxelBlockyFluid::VoxelBlockyFluid() {}

void VoxelBlockyFluid::set_material(Ref<Material> material) {
	if (_material == material) {
		return;
	}
	_material = material;
	emit_changed();
}

Ref<Material> VoxelBlockyFluid::get_material() const {
	return _material;
}

namespace {

void bake_fluid(
		const VoxelBlockyFluid &fluid,
		VoxelBlockyFluid::BakedData &baked_fluid,
		blocky::MaterialIndexer &materials
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
		VoxelBlockyFluid::Surface &surface = baked_fluid.side_surfaces[side_index];
		make_cube_side_vertices(surface.positions, side_index, VoxelBlockyFluid::BakedData::TOP_HEIGHT);
		// make_cube_side_tangents(ss.tangents, side_index);
		make_cube_side_indices(surface.indices, side_index);
	}
}

} // namespace

void VoxelBlockyFluid::bake(VoxelBlockyFluid::BakedData &baked_fluid, blocky::MaterialIndexer &materials) const {
	bake_fluid(*this, baked_fluid, materials);
}

void VoxelBlockyFluid::_bind_methods() {
	using Self = VoxelBlockyFluid;

	ClassDB::bind_method(D_METHOD("set_material", "material"), &Self::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Self::get_material);

	ADD_PROPERTY(
			PropertyInfo(
					Variant::OBJECT,
					"material",
					PROPERTY_HINT_RESOURCE_TYPE,
					zylann::godot::MATERIAL_3D_PROPERTY_HINT_STRING
			),
			"set_material",
			"get_material"
	);
}

} // namespace zylann::voxel
