#include "direct_mesh_instance.h"
#include "../profiling.h"
#include "classes/material.h"
#include "classes/world_3d.h"

namespace zylann {

DirectMeshInstance::DirectMeshInstance() {
	// Nothing here. It is a thin RID wrapper,
	// no calls to RenderingServer are made until we called one of the functions.
}

DirectMeshInstance::DirectMeshInstance(DirectMeshInstance &&src) {
	_mesh_instance = src._mesh_instance;
	_mesh = src._mesh;

	src._mesh_instance = RID();
	src._mesh = Ref<Mesh>();
}

DirectMeshInstance::~DirectMeshInstance() {
	destroy();
}

bool DirectMeshInstance::is_valid() const {
	return _mesh_instance.is_valid();
}

void DirectMeshInstance::create() {
	ERR_FAIL_COND(_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	_mesh_instance = vs.instance_create();
	vs.instance_set_visible(_mesh_instance, true); // TODO Is it needed?
}

void DirectMeshInstance::destroy() {
	if (_mesh_instance.is_valid()) {
		ZN_PROFILE_SCOPE();
		RenderingServer &vs = *RenderingServer::get_singleton();
		free_rendering_server_rid(vs, _mesh_instance);
		_mesh_instance = RID();
	}
	_mesh.unref();
}

void DirectMeshInstance::set_world(World3D *world) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	if (world != nullptr) {
		vs.instance_set_scenario(_mesh_instance, world->get_scenario());
	} else {
		vs.instance_set_scenario(_mesh_instance, RID());
	}
}

void DirectMeshInstance::set_transform(Transform3D world_transform) {
	ZN_PROFILE_SCOPE();
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_set_transform(_mesh_instance, world_transform);
}

void DirectMeshInstance::set_mesh(Ref<Mesh> mesh) {
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

void DirectMeshInstance::set_material_override(Ref<Material> material) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	if (material.is_valid()) {
		vs.instance_geometry_set_material_override(_mesh_instance, material->get_rid());
	} else {
		vs.instance_geometry_set_material_override(_mesh_instance, RID());
	}
}

void DirectMeshInstance::set_visible(bool visible) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_set_visible(_mesh_instance, visible);
}

void DirectMeshInstance::set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_geometry_set_cast_shadows_setting(_mesh_instance, mode);
}

void DirectMeshInstance::set_shader_instance_parameter(StringName key, Variant value) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();
	vs.instance_geometry_set_shader_parameter(_mesh_instance, key, value);
}

Ref<Mesh> DirectMeshInstance::get_mesh() const {
	return _mesh;
}

void DirectMeshInstance::set_gi_mode(GIMode mode) {
	ERR_FAIL_COND(!_mesh_instance.is_valid());
	RenderingServer &vs = *RenderingServer::get_singleton();

	bool baked_light;
	bool dynamic_gi;

	switch (mode) {
		case GI_MODE_DISABLED:
			baked_light = false;
			dynamic_gi = false;
			break;
		case GI_MODE_BAKED:
			baked_light = true;
			dynamic_gi = false;
			break;
		case GI_MODE_DYNAMIC:
			baked_light = false;
			dynamic_gi = true;
			break;
		default:
			CRASH_NOW_MSG("Unexpected GIMode");
	}

	vs.instance_geometry_set_flag(_mesh_instance, RenderingServer::INSTANCE_FLAG_USE_BAKED_LIGHT, baked_light);
	vs.instance_geometry_set_flag(_mesh_instance, RenderingServer::INSTANCE_FLAG_USE_DYNAMIC_GI, dynamic_gi);
}

void DirectMeshInstance::operator=(DirectMeshInstance &&src) {
	if (_mesh_instance == src._mesh_instance) {
		return;
	}

	destroy();

	_mesh_instance = src._mesh_instance;
	_mesh = src._mesh;

	src._mesh_instance = RID();
	src._mesh.unref();
}

} // namespace zylann
