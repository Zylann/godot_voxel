#include "direct_multimesh_instance.h"
#include "profiling.h"

#include <scene/resources/world.h>

DirectMultiMeshInstance::DirectMultiMeshInstance() {
}

DirectMultiMeshInstance::~DirectMultiMeshInstance() {
	destroy();
}

bool DirectMultiMeshInstance::is_valid() const {
	return _multimesh_instance.is_valid();
}

void DirectMultiMeshInstance::create() {
	ERR_FAIL_COND(_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	_multimesh_instance = vs.instance_create();
	vs.instance_set_visible(_multimesh_instance, true); // TODO Is it needed?
}

void DirectMultiMeshInstance::destroy() {
	if (_multimesh_instance.is_valid()) {
		VisualServer &vs = *VisualServer::get_singleton();
		vs.free(_multimesh_instance);
		_multimesh_instance = RID();
		_multimesh.unref();
	}
}

void DirectMultiMeshInstance::set_world(World *world) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (world != nullptr) {
		vs.instance_set_scenario(_multimesh_instance, world->get_scenario());
	} else {
		vs.instance_set_scenario(_multimesh_instance, RID());
	}
}

void DirectMultiMeshInstance::set_multimesh(Ref<MultiMesh> multimesh) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (multimesh.is_valid()) {
		if (_multimesh != multimesh) {
			vs.instance_set_base(_multimesh_instance, multimesh->get_rid());
		}
	} else {
		vs.instance_set_base(_multimesh_instance, RID());
	}
	_multimesh = multimesh;
}

Ref<MultiMesh> DirectMultiMeshInstance::get_multimesh() const {
	return _multimesh;
}

void DirectMultiMeshInstance::set_transform(Transform world_transform) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_set_transform(_multimesh_instance, world_transform);
}

void DirectMultiMeshInstance::set_visible(bool visible) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_set_visible(_multimesh_instance, visible);
}
