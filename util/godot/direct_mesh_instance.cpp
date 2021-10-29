#include "direct_mesh_instance.h"
#include "../profiling.h"
#include <scene/resources/world_3d.h>

DirectMeshInstance3D::DirectMeshInstance3D() {
}

DirectMeshInstance3D::~DirectMeshInstance3D() {
	destroy();
}

bool DirectMeshInstance3D::is_valid() const {
	return _mesh_instance.is_valid();
}

void DirectMeshInstance3D::create() {
	ERR_FAIL_COND(_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	_mesh_instance = vs.instance_create();
	vs.instance_set_visible(_mesh_instance, true); // TODO Is it needed?
}

void DirectMeshInstance3D::destroy() {
	if (_mesh_instance.is_valid()) {
		RenderingServer &vs = *RenderingServer::get_singleton();
		vs.free(_mesh_instance);
		_mesh_instance = RID();
		_mesh.unref();
	}
}

void DirectMeshInstance3D::set_world(World3D *world) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	if (world != nullptr) {
		vs.instance_set_scenario(_mesh_instance, world->get_scenario());
	} else {
		vs.instance_set_scenario(_mesh_instance, RID());
	}
}

void DirectMeshInstance3D::set_transform(Transform3D world_transform) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_set_transform(_mesh_instance, world_transform);
}

void DirectMeshInstance3D::set_mesh(Ref<Mesh> mesh) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	if (mesh.is_valid()) {
		if (_mesh != mesh) {
			vs.instance_set_base(_mesh_instance, mesh->get_rid());
		}
	} else {
		vs.instance_set_base(_mesh_instance, RID());
	}
	_mesh = mesh;
}

void DirectMeshInstance3D::set_material_override(Ref<Material> material) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	if (material.is_valid()) {
		vs.instance_geometry_set_material_override(_mesh_instance, material->get_rid());
	} else {
		vs.instance_geometry_set_material_override(_mesh_instance, RID());
	}
}

void DirectMeshInstance3D::set_visible(bool visible) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_set_visible(_mesh_instance, visible);
}

void DirectMeshInstance3D::set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_geometry_set_cast_shadows_setting(_mesh_instance, mode);
}

Ref<Mesh> DirectMeshInstance3D::get_mesh() const {
	return _mesh;
}
