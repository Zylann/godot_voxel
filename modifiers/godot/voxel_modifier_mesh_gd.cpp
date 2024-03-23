#include "voxel_modifier_mesh_gd.h"
#include "../../util/godot/core/array.h"
#include "../voxel_modifier_mesh.h"

namespace zylann::voxel::godot {

zylann::voxel::VoxelModifierMesh *get_mesh_modifier(VoxelLodTerrain &volume, uint32_t id) {
	return get_modifier<zylann::voxel::VoxelModifierMesh>(volume, id, zylann::voxel::VoxelModifier::TYPE_MESH);
}

void VoxelModifierMesh::set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf) {
	if (mesh_sdf == _mesh_sdf) {
		return;
	}
	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->disconnect("baked", callable_mp(this, &VoxelModifierMesh::_on_mesh_sdf_baked));
	}
	_mesh_sdf = mesh_sdf;
	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->connect("baked", callable_mp(this, &VoxelModifierMesh::_on_mesh_sdf_baked));
	}
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierMesh *modifier = get_mesh_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(modifier != nullptr);
	const AABB prev_aabb = modifier->get_aabb();
	modifier->set_mesh_sdf(_mesh_sdf);
	const AABB new_aabb = modifier->get_aabb();
	post_edit_modifier(*_volume, prev_aabb);
	post_edit_modifier(*_volume, new_aabb);
	update_configuration_warnings();
}

Ref<VoxelMeshSDF> VoxelModifierMesh::get_mesh_sdf() const {
	return _mesh_sdf;
}

void VoxelModifierMesh::set_isolevel(float isolevel) {
	if (isolevel == _isolevel) {
		return;
	}
	_isolevel = isolevel;
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierMesh *modifier = get_mesh_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(modifier != nullptr);
	modifier->set_isolevel(_isolevel);
	post_edit_modifier(*_volume, modifier->get_aabb());
}

float VoxelModifierMesh::get_isolevel() const {
	return _isolevel;
}

zylann::voxel::VoxelModifier *VoxelModifierMesh::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	zylann::voxel::VoxelModifierMesh *modifier = modifiers.add_modifier<zylann::voxel::VoxelModifierMesh>(id);
	modifier->set_mesh_sdf(_mesh_sdf);
	modifier->set_isolevel(_isolevel);
	return modifier;
}

void VoxelModifierMesh::_on_mesh_sdf_baked() {
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierMesh *modifier = get_mesh_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(modifier != nullptr);
	const AABB prev_aabb = modifier->get_aabb();
	modifier->set_mesh_sdf(_mesh_sdf);
	const AABB new_aabb = modifier->get_aabb();
	post_edit_modifier(*_volume, prev_aabb);
	post_edit_modifier(*_volume, new_aabb);
	update_configuration_warnings();
}

#ifdef TOOLS_ENABLED

void VoxelModifierMesh::get_configuration_warnings(PackedStringArray &warnings) const {
	if (_mesh_sdf.is_null()) {
		warnings.append(
				ZN_TTR("A {0} resource is required for {1} to function.")
						.format(varray(VoxelMeshSDF::get_class_static(), VoxelModifierMesh::get_class_static())));
	} else {
		if (_mesh_sdf->get_mesh().is_null()) {
			warnings.append(
					ZN_TTR("The {0} resource has no mesh assigned.").format(varray(VoxelMeshSDF::get_class_static())));

		} else if (!_mesh_sdf->is_baked()) {
			warnings.append(
					ZN_TTR("The {0} resource needs to be baked.").format(varray(VoxelMeshSDF::get_class_static())));
		}
	}
}

#endif

void VoxelModifierMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_sdf", "mesh_sdf"), &VoxelModifierMesh::set_mesh_sdf);
	ClassDB::bind_method(D_METHOD("get_mesh_sdf"), &VoxelModifierMesh::get_mesh_sdf);

	ClassDB::bind_method(D_METHOD("set_isolevel", "isolevel"), &VoxelModifierMesh::set_isolevel);
	ClassDB::bind_method(D_METHOD("get_isolevel"), &VoxelModifierMesh::get_isolevel);

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "mesh_sdf", PROPERTY_HINT_RESOURCE_TYPE, VoxelMeshSDF::get_class_static()),
			"set_mesh_sdf", "get_mesh_sdf");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "isolevel", PROPERTY_HINT_RANGE, "-100.0, 100.0, 0.01"), "set_isolevel",
			"get_isolevel");
}

} // namespace zylann::voxel::godot
