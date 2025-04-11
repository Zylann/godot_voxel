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

void VoxelBlockyFluid::set_dip_when_flowing_down(bool enable) {
	_dip_when_flowing_down = enable;
}

bool VoxelBlockyFluid::get_dip_when_flowing_down() const {
	return _dip_when_flowing_down;
}

namespace blocky {

void bake_fluid(const VoxelBlockyFluid &fluid, BakedFluid &baked_fluid, MaterialIndexer &materials) {
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
	baked_fluid.dip_when_flowing_down = fluid.get_dip_when_flowing_down();

	// TODO This part shouldn't be necessary? it's the same for every fluid
	for (unsigned int side_index = 0; side_index < Cube::SIDE_COUNT; ++side_index) {
		BakedFluid::Surface &surface = baked_fluid.side_surfaces[side_index];
		make_cube_side_vertices(surface.positions, side_index, 1.f);
		// make_cube_side_tangents(ss.tangents, side_index);
		make_cube_side_indices(surface.indices, side_index);
	}
}

} // namespace blocky

void VoxelBlockyFluid::bake(blocky::BakedFluid &baked_fluid, blocky::MaterialIndexer &materials) const {
	bake_fluid(*this, baked_fluid, materials);
}

void VoxelBlockyFluid::_bind_methods() {
	using Self = VoxelBlockyFluid;

	ClassDB::bind_method(D_METHOD("set_material", "material"), &Self::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &Self::get_material);

	ClassDB::bind_method(D_METHOD("set_dip_when_flowing_down", "enable"), &Self::set_dip_when_flowing_down);
	ClassDB::bind_method(D_METHOD("get_dip_when_flowing_down"), &Self::get_dip_when_flowing_down);

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

	ADD_PROPERTY(
			PropertyInfo(Variant::BOOL, "dip_when_flowing_down"),
			"set_dip_when_flowing_down",
			"get_dip_when_flowing_down"
	);
}

} // namespace zylann::voxel
