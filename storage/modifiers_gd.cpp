#include "modifiers_gd.h"
#include "../terrain/variable_lod/voxel_lod_terrain.h"
#include "../util/errors.h"
#include "../util/godot/callable.h"
#include "../util/godot/node.h"
#include "../util/math/conv.h"

namespace zylann::voxel::gd {

void post_edit_modifier(VoxelLodTerrain &volume, AABB aabb) {
	volume.post_edit_modifiers(Box3i(math::floor_to_int(aabb.position), math::floor_to_int(aabb.size)));
}

// template <typename Modifier_T, typename F>
// void edit_modifier(VoxelLodTerrain &volume, Modifier_T &modifier, F action) {
// 	const AABB prev_aabb = modifier->get_aabb();
// 	action(modifier);
// 	const AABB new_aabb = modifier->get_aabb();
// 	post_edit_modifier(volume, prev_aabb);
// 	post_edit_modifier(volume, new_aabb);
// }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VoxelModifier::VoxelModifier() {
	set_notify_local_transform(true);
}

zylann::voxel::VoxelModifier *VoxelModifier::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	ZN_PRINT_ERROR("Not implemented");
	return nullptr;
}

zylann::voxel::VoxelModifierSdf::Operation to_op(VoxelModifier::Operation op) {
	return zylann::voxel::VoxelModifierSdf::Operation(op);
}

void VoxelModifier::set_operation(Operation op) {
	ZN_ASSERT_RETURN(op >= 0 && op < OPERATION_COUNT);
	if (op == _operation) {
		return;
	}
	_operation = op;
	if (_volume == nullptr) {
		return;
	}
	VoxelData &data = _volume->get_storage();
	VoxelModifierStack &modifiers = data.get_modifiers();
	zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
	ZN_ASSERT_RETURN(modifier != nullptr);
	ZN_ASSERT_RETURN(modifier->is_sdf());
	zylann::voxel::VoxelModifierSdf *sdf_modifier = static_cast<zylann::voxel::VoxelModifierSdf *>(modifier);
	sdf_modifier->set_operation(to_op(_operation));
	post_edit_modifier(*_volume, modifier->get_aabb());
}

VoxelModifier::Operation VoxelModifier::get_operation() const {
	return _operation;
}

void VoxelModifier::set_smoothness(float s) {
	if (s == _smoothness) {
		return;
	}
	_smoothness = math::max(s, 0.f);
	if (_volume == nullptr) {
		return;
	}
	VoxelData &data = _volume->get_storage();
	VoxelModifierStack &modifiers = data.get_modifiers();
	zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
	ZN_ASSERT_RETURN(modifier != nullptr);
	ZN_ASSERT_RETURN(modifier->is_sdf());
	zylann::voxel::VoxelModifierSdf *sdf_modifier = static_cast<zylann::voxel::VoxelModifierSdf *>(modifier);
	const AABB prev_aabb = modifier->get_aabb();
	sdf_modifier->set_smoothness(_smoothness);
	const AABB new_aabb = modifier->get_aabb();
	post_edit_modifier(*_volume, prev_aabb);
	post_edit_modifier(*_volume, new_aabb);
}

float VoxelModifier::get_smoothness() const {
	return _smoothness;
}

void VoxelModifier::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			Node *parent = get_parent();
			ZN_ASSERT_RETURN(parent != nullptr);
			ZN_ASSERT_RETURN(_volume == nullptr);
			VoxelLodTerrain *volume = Object::cast_to<VoxelLodTerrain>(parent);
			_volume = volume;

			if (_volume != nullptr) {
				VoxelData &data = _volume->get_storage();
				VoxelModifierStack &modifiers = data.get_modifiers();
				const uint32_t id = modifiers.allocate_id();
				zylann::voxel::VoxelModifier *modifier = create(modifiers, id);

				if (modifier->is_sdf()) {
					zylann::voxel::VoxelModifierSdf *sdf_modifier =
							static_cast<zylann::voxel::VoxelModifierSdf *>(modifier);
					sdf_modifier->set_operation(to_op(_operation));
					sdf_modifier->set_smoothness(_smoothness);
				}

				modifier->set_transform(get_transform());
				_modifier_id = id;
				// TODO Optimize: on loading of a scene, this could be very bad for performance because there could be,
				// a lot of modifiers on the map, but there is no distinction possible in Godot at the moment...
				post_edit_modifier(*_volume, modifier->get_aabb());
			}
		} break;

		case NOTIFICATION_UNPARENTED: {
			if (_volume != nullptr) {
				VoxelData &data = _volume->get_storage();
				VoxelModifierStack &modifiers = data.get_modifiers();
				zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
				ZN_ASSERT_RETURN_MSG(modifier != nullptr, "The modifier node wasn't linked properly");
				post_edit_modifier(*_volume, modifier->get_aabb());
				modifiers.remove_modifier(_modifier_id);
				_volume = nullptr;
				_modifier_id = 0;
			}
		} break;

		//case Node3D::NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
		case zylann::NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (_volume != nullptr && is_inside_tree()) {
				VoxelData &data = _volume->get_storage();
				VoxelModifierStack &modifiers = data.get_modifiers();
				zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
				ZN_ASSERT_RETURN(modifier != nullptr);

				const AABB prev_aabb = modifier->get_aabb();
				modifier->set_transform(get_transform());
				const AABB aabb = modifier->get_aabb();
				post_edit_modifier(*_volume, prev_aabb);
				post_edit_modifier(*_volume, aabb);

				// TODO Handle nesting properly, though it's a pain in the ass
				// When the terrain is moved, the local transform of modifiers technically changes too.
				// However it did not change relative to the terrain. But because we don't have a way to check that,
				// all modifiers will trigger updates at the same time...
			}
		} break;
	}
}

void VoxelModifier::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_operation", "op"), &VoxelModifier::set_operation);
	ClassDB::bind_method(D_METHOD("get_operation"), &VoxelModifier::get_operation);

	ClassDB::bind_method(D_METHOD("set_smoothness", "smoothness"), &VoxelModifier::set_smoothness);
	ClassDB::bind_method(D_METHOD("get_smoothness"), &VoxelModifier::get_smoothness);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "operation", PROPERTY_HINT_ENUM, "Add,Remove"), "set_operation",
			"get_operation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "smoothness", PROPERTY_HINT_RANGE, "0.0, 100.0, 0.1"), "set_smoothness",
			"get_smoothness");

	BIND_ENUM_CONSTANT(OPERATION_ADD);
	BIND_ENUM_CONSTANT(OPERATION_REMOVE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T *get_modifier(VoxelLodTerrain &volume, uint32_t id, zylann::voxel::VoxelModifier::Type type) {
	VoxelData &data = volume.get_storage();
	VoxelModifierStack &modifiers = data.get_modifiers();
	zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(id);
	ZN_ASSERT_RETURN_V(modifier != nullptr, nullptr);
	ZN_ASSERT_RETURN_V(modifier->get_type() == type, nullptr);
	return static_cast<T *>(modifier);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

zylann::voxel::VoxelModifierSphere *get_sphere(VoxelLodTerrain &volume, uint32_t id) {
	return get_modifier<zylann::voxel::VoxelModifierSphere>(volume, id, zylann::voxel::VoxelModifier::TYPE_SPHERE);
}

float VoxelModifierSphere::get_radius() const {
	return _radius;
}

void VoxelModifierSphere::set_radius(float r) {
	_radius = math::max(r, 0.f);
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierSphere *sphere = get_sphere(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(sphere != nullptr);
	const AABB prev_aabb = sphere->get_aabb();
	sphere->set_radius(r);
	const AABB new_aabb = sphere->get_aabb();
	post_edit_modifier(*_volume, prev_aabb);
	post_edit_modifier(*_volume, new_aabb);
}

zylann::voxel::VoxelModifier *VoxelModifierSphere::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	zylann::voxel::VoxelModifierSphere *sphere = modifiers.add_modifier<zylann::voxel::VoxelModifierSphere>(id);
	sphere->set_radius(_radius);
	return sphere;
}

void VoxelModifierSphere::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "r"), &VoxelModifierSphere::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &VoxelModifierSphere::get_radius);

	ADD_PROPERTY(
			PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.0, 100.0, 0.1"), "set_radius", "get_radius");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

zylann::voxel::VoxelModifierBuffer *get_buffer_modifier(VoxelLodTerrain &volume, uint32_t id) {
	return get_modifier<zylann::voxel::VoxelModifierBuffer>(volume, id, zylann::voxel::VoxelModifier::TYPE_BUFFER);
}

static void set_buffer(zylann::voxel::VoxelModifierBuffer &bmod, Ref<VoxelMeshSDF> mesh_sdf) {
	if (mesh_sdf.is_null() || mesh_sdf->get_voxel_buffer() == nullptr) {
		bmod.set_buffer(nullptr, Vector3f(), Vector3f());
	} else {
		const AABB aabb = mesh_sdf->get_aabb();
		bmod.set_buffer(mesh_sdf->get_voxel_buffer()->get_buffer_shared(), to_vec3f(aabb.position),
				to_vec3f(aabb.position + aabb.size));
	}
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
	zylann::voxel::VoxelModifierBuffer *bmod = get_buffer_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(bmod != nullptr);
	const AABB prev_aabb = bmod->get_aabb();
	set_buffer(*bmod, _mesh_sdf);
	const AABB new_aabb = bmod->get_aabb();
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
	zylann::voxel::VoxelModifierBuffer *bmod = get_buffer_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(bmod != nullptr);
	bmod->set_isolevel(_isolevel);
	post_edit_modifier(*_volume, bmod->get_aabb());
}

float VoxelModifierMesh::get_isolevel() const {
	return _isolevel;
}

zylann::voxel::VoxelModifier *VoxelModifierMesh::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	zylann::voxel::VoxelModifierBuffer *bmod = modifiers.add_modifier<zylann::voxel::VoxelModifierBuffer>(id);
	set_buffer(*bmod, _mesh_sdf);
	bmod->set_isolevel(_isolevel);
	return bmod;
}

void VoxelModifierMesh::_on_mesh_sdf_baked() {
	if (_volume == nullptr) {
		return;
	}
	zylann::voxel::VoxelModifierBuffer *bmod = get_buffer_modifier(*_volume, _modifier_id);
	ZN_ASSERT_RETURN(bmod != nullptr);
	const AABB prev_aabb = bmod->get_aabb();
	set_buffer(*bmod, _mesh_sdf);
	const AABB new_aabb = bmod->get_aabb();
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
