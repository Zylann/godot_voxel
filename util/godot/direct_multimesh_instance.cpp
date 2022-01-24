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


void DirectMultiMeshInstance::set_portal_mode(VisualServer::InstancePortalMode mode) {
	ERR_FAIL_COND(!_multimesh_instance.is_valid());
	VisualServer &vs = *VisualServer::get_singleton();
	vs.instance_set_portal_mode(_multimesh_instance, mode);
}

inline void write_bulk_array_transform(float *dst, const Transform &t) {
	dst[0] = t.basis.elements[0].x;
	dst[1] = t.basis.elements[0].y;
	dst[2] = t.basis.elements[0].z;
	dst[3] = t.origin.x;

	dst[4] = t.basis.elements[1].x;
	dst[5] = t.basis.elements[1].y;
	dst[6] = t.basis.elements[1].z;
	dst[7] = t.origin.y;

	dst[8] = t.basis.elements[2].x;
	dst[9] = t.basis.elements[2].y;
	dst[10] = t.basis.elements[2].z;
	dst[11] = t.origin.z;
}

void DirectMultiMeshInstance::make_transform_3d_bulk_array(
		Span<const Transform> transforms, PoolRealArray &bulk_array) {
	VOXEL_PROFILE_SCOPE();

	const int item_size = 12; // In number of floats

	const unsigned int bulk_array_size = transforms.size() * item_size;
	if (static_cast<unsigned int>(bulk_array.size()) != bulk_array_size) {
		bulk_array.resize(bulk_array_size);
	}
	CRASH_COND(transforms.size() * sizeof(Transform) / sizeof(float) != static_cast<size_t>(bulk_array.size()));

	PoolRealArray::Write w = bulk_array.write();

	//memcpy(w.ptr(), _transform_cache.data(), bulk_array.size() * sizeof(float));
	// Nope, you can't memcpy that, nonono. It's said to be for performance, but doesnt specify why.

	for (size_t i = 0; i < transforms.size(); ++i) {
		float *ptr = w.ptr() + item_size * i;
		const Transform &t = transforms[i];
		write_bulk_array_transform(ptr, t);
	}
}

void DirectMultiMeshInstance::make_transform_and_color8_3d_bulk_array(
		Span<const TransformAndColor8> data, PoolRealArray &bulk_array) {
	VOXEL_PROFILE_SCOPE();

	const int transform_size = 12; // In number of floats
	const int item_size = transform_size + 1;

	const unsigned int bulk_array_size = data.size() * item_size;
	if (static_cast<unsigned int>(bulk_array.size()) != bulk_array_size) {
		bulk_array.resize(bulk_array_size);
	}
	CRASH_COND(data.size() * sizeof(TransformAndColor8) / sizeof(float) != static_cast<size_t>(bulk_array.size()));

	PoolRealArray::Write w = bulk_array.write();

	for (size_t i = 0; i < data.size(); ++i) {
		float *ptr = w.ptr() + item_size * i;
		const TransformAndColor8 &d = data[i];
		write_bulk_array_transform(ptr, d.transform);
		ptr[transform_size] = *reinterpret_cast<const float *>(d.color.components);
	}
}
