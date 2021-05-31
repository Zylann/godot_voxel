#include "direct_multimesh_instance.h"
#include "../profiling.h"

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

void DirectMultiMeshInstance::set_material_override(Ref<Material> material) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	if (material.is_valid()) {
		vs.instance_geometry_set_material_override(_multimesh_instance, material->get_rid());
	} else {
		vs.instance_geometry_set_material_override(_multimesh_instance, RID());
	}
}

void DirectMultiMeshInstance::set_cast_shadows_setting(VisualServer::ShadowCastingSetting mode) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_geometry_set_cast_shadows_setting(_multimesh_instance, mode);
}

PoolRealArray DirectMultiMeshInstance::make_transform_3d_bulk_array(Span<const Transform> transforms) {
	VOXEL_PROFILE_SCOPE();

	PoolRealArray bulk_array;
	bulk_array.resize(transforms.size() * 12);
	CRASH_COND(transforms.size() * sizeof(Transform) / sizeof(float) != static_cast<size_t>(bulk_array.size()));

	PoolRealArray::Write w = bulk_array.write();

	//memcpy(w.ptr(), _transform_cache.data(), bulk_array.size() * sizeof(float));
	// Nope, you can't memcpy that, nonono. It's said to be for performance, but doesnt specify why.

	for (size_t i = 0; i < transforms.size(); ++i) {
		float *ptr = w.ptr() + 12 * i;
		const Transform &t = transforms[i];

		ptr[0] = t.basis.elements[0].x;
		ptr[1] = t.basis.elements[0].y;
		ptr[2] = t.basis.elements[0].z;
		ptr[3] = t.origin.x;

		ptr[4] = t.basis.elements[1].x;
		ptr[5] = t.basis.elements[1].y;
		ptr[6] = t.basis.elements[1].z;
		ptr[7] = t.origin.y;

		ptr[8] = t.basis.elements[2].x;
		ptr[9] = t.basis.elements[2].y;
		ptr[10] = t.basis.elements[2].z;
		ptr[11] = t.origin.z;
	}

	return bulk_array;
}
