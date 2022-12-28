#include "voxel_modifier_mesh_gd.h"
#include "../../util/godot/core/callable.h"
#include "../voxel_modifier_mesh.h"

namespace zylann::voxel::gd {

zylann::voxel::VoxelModifierMesh *get_mesh_modifier(VoxelLodTerrain &volume, uint32_t id) {
	return get_modifier<zylann::voxel::VoxelModifierMesh>(volume, id, zylann::voxel::VoxelModifier::TYPE_MESH);
}

void VoxelModifierMesh::set_mesh_sdf(Ref<VoxelMeshSDF> mesh_sdf) {
	if (mesh_sdf == _mesh_sdf) {
		return;
	}
	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->disconnect("baked", ZN_GODOT_CALLABLE_MP(this, VoxelModifierMesh, _on_mesh_sdf_baked));
	}
	_mesh_sdf = mesh_sdf;
	if (_mesh_sdf.is_valid()) {
		_mesh_sdf->connect("baked", ZN_GODOT_CALLABLE_MP(this, VoxelModifierMesh, _on_mesh_sdf_baked));
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
}

void VoxelModifierMesh::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_sdf", "mesh_sdf"), &VoxelModifierMesh::set_mesh_sdf);
	ClassDB::bind_method(D_METHOD("get_mesh_sdf"), &VoxelModifierMesh::get_mesh_sdf);

	ClassDB::bind_method(D_METHOD("set_isolevel", "isolevel"), &VoxelModifierMesh::set_isolevel);
	ClassDB::bind_method(D_METHOD("get_isolevel"), &VoxelModifierMesh::get_isolevel);

#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_mesh_sdf_baked"), &VoxelModifierMesh::_on_mesh_sdf_baked);
#endif

	ADD_PROPERTY(
			PropertyInfo(Variant::OBJECT, "mesh_sdf", PROPERTY_HINT_RESOURCE_TYPE, VoxelMeshSDF::get_class_static()),
			"set_mesh_sdf", "get_mesh_sdf");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "isolevel", PROPERTY_HINT_RANGE, "-100.0, 100.0, 0.01"), "set_isolevel",
			"get_isolevel");
}

} // namespace zylann::voxel::gd
