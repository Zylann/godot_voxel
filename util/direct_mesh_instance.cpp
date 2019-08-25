#include "direct_mesh_instance.h"
#include <scene/resources/world.h>

DirectMeshInstance::DirectMeshInstance() {
}

DirectMeshInstance::~DirectMeshInstance() {
	destroy();
}

bool DirectMeshInstance::is_valid() const {
	return _mesh_instance.is_valid();
}

void DirectMeshInstance::create() {
	ERR_FAIL_COND(_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	_mesh_instance = vs.instance_create();
	vs.instance_set_visible(_mesh_instance, true); // TODO Is it needed?
}

void DirectMeshInstance::destroy() {
	if (_mesh_instance.is_valid()) {
		VisualServer &vs = *VisualServer::get_singleton();
		vs.free(_mesh_instance);
		_mesh_instance = RID();
		_mesh.unref();
	}
}

void DirectMeshInstance::set_world(World *world) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (world != nullptr) {
		vs.instance_set_scenario(_mesh_instance, world->get_scenario());
	} else {
		vs.instance_set_scenario(_mesh_instance, RID());
	}
}

void DirectMeshInstance::set_transform(Transform world_transform) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_set_transform(_mesh_instance, world_transform);
}

void DirectMeshInstance::set_mesh(Ref<Mesh> mesh) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (mesh.is_valid()) {
		vs.instance_set_base(_mesh_instance, mesh->get_rid());
	} else {
		vs.instance_set_base(_mesh_instance, RID());
	}
	_mesh = mesh;
}

void DirectMeshInstance::set_material_override(Ref<Material> material) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (material.is_valid()) {
		vs.instance_geometry_set_material_override(_mesh_instance, material->get_rid());
	} else {
		vs.instance_geometry_set_material_override(_mesh_instance, RID());
	}
}

void DirectMeshInstance::set_visible(bool visible) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_set_visible(_mesh_instance, visible);
}

Ref<Mesh> DirectMeshInstance::get_mesh() const {
	return _mesh;
}
