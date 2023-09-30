#include "voxel_modifier_gd.h"
#include "../../util/godot/core/array.h"
#include "../../util/log.h"
#include "../../util/math/conv.h"
#include "../voxel_modifier_sdf.h"

namespace zylann::voxel::gd {

VoxelModifier::VoxelModifier() { }

zylann::voxel::VoxelModifier *VoxelModifier::create(zylann::voxel::VoxelModifierStack &modifiers, uint32_t id) {
	ZN_PRINT_ERROR("Not implemented");
	return nullptr;
}

zylann::voxel::VoxelModifierSdf::Operation to_op(VoxelModifier::Operation op) {
	return zylann::voxel::VoxelModifierSdf::Operation(op);
}

void post_edit_modifier(VoxelLodTerrain &volume, AABB aabb) {
	volume.post_edit_modifiers(Box3i(math::floor_to_int(aabb.position), math::floor_to_int(aabb.size)));
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
	VoxelData &voxel_data = _volume->get_storage();
	VoxelModifierStack &modifiers = voxel_data.get_modifiers();
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
	VoxelData &voxel_data = _volume->get_storage();
	VoxelModifierStack &modifiers = voxel_data.get_modifiers();
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

// Not sure where the best place to put this is, probably one of the math files?
#include <core/math/math_funcs.h>
bool transform3d_is_equal_approx(Transform3D a, Transform3D b) {
	return Math::is_equal_approx(b.basis.rows[0].x, b.basis.rows[0].x) && 
		   Math::is_equal_approx(a.basis.rows[0].y, b.basis.rows[0].y) && 
		   Math::is_equal_approx(a.basis.rows[0].z, b.basis.rows[0].z) && 
		   Math::is_equal_approx(a.basis.rows[1].x, b.basis.rows[1].x) && 
		   Math::is_equal_approx(a.basis.rows[1].y, b.basis.rows[1].y) && 
		   Math::is_equal_approx(a.basis.rows[1].z, b.basis.rows[1].z) && 
		   Math::is_equal_approx(a.basis.rows[2].x, b.basis.rows[2].x) && 
		   Math::is_equal_approx(a.basis.rows[2].y, b.basis.rows[2].y) && 
		   Math::is_equal_approx(a.basis.rows[2].z, b.basis.rows[2].z) && 
		   Math::is_equal_approx(a.origin.x, b.origin.x) && 
		   Math::is_equal_approx(a.origin.y, b.origin.y) && 
		   Math::is_equal_approx(a.origin.z, b.origin.z);
}

VoxelLodTerrain *VoxelModifier::find_volume() {
	if (_volume != nullptr) {
		return _volume;
	}
	Node *parent = get_parent();
	VoxelLodTerrain *volume = Object::cast_to<VoxelLodTerrain>(parent);
	
	if (volume != nullptr) {
		mark_as_immediate_child(true);
	} else {
		Node3D *grandparent = get_parent_node_3d();
		volume = Object::cast_to<VoxelLodTerrain>(grandparent);
		while (grandparent != nullptr && volume == nullptr) {				
			grandparent = grandparent->get_parent_node_3d();
			volume = Object::cast_to<VoxelLodTerrain>(grandparent);
		}
		if (volume) {
			mark_as_immediate_child(false);
		}
	}
	_volume = volume;

	if (_volume == nullptr) {
		return nullptr;
	}
	VoxelData &voxel_data = _volume->get_storage();
	VoxelModifierStack &modifiers = voxel_data.get_modifiers();
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
	update_configuration_warnings();
	return _volume;
}

void VoxelModifier::update_volume() {
	VoxelLodTerrain *volume = find_volume();

	if (volume != nullptr && is_inside_tree()) {
		VoxelData &voxel_data = volume->get_storage();
		VoxelModifierStack &modifiers = voxel_data.get_modifiers();
		zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
		ZN_ASSERT_RETURN(modifier != nullptr);

		const AABB prev_aabb = modifier->get_aabb();

		if (_is_immediate_child) {
			modifier->set_transform(get_transform());
		} else {
			Transform3D terrain_local = volume->get_global_transform().affine_inverse() * get_global_transform();
			// If the terrain moved, the modifiers do not need to be updated.
			if (transform3d_is_equal_approx(terrain_local, modifier->get_transform())) {
				return;
			}
			modifier->set_transform(terrain_local);
		}
		WARN_PRINT("MODIFIER WAS UPDATED");
		const AABB aabb = modifier->get_aabb();
		post_edit_modifier(*volume, prev_aabb);
		post_edit_modifier(*volume, aabb);
	}
}

void VoxelModifier::mark_as_immediate_child(bool v) {
	_is_immediate_child = v;
	if (_is_immediate_child) {
		set_notify_local_transform(true);
		set_notify_transform(false);
	} else {
		set_notify_local_transform(false);
		set_notify_transform(true);
	}
}

void VoxelModifier::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			find_volume();
		} break;
		case NOTIFICATION_UNPARENTED: {
			if (_volume != nullptr) {
				VoxelData &voxel_data = _volume->get_storage();
				VoxelModifierStack &modifiers = voxel_data.get_modifiers();
				zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
				ZN_ASSERT_RETURN_MSG(modifier != nullptr, "The modifier node wasn't linked properly");
				post_edit_modifier(*_volume, modifier->get_aabb());
				modifiers.remove_modifier(_modifier_id);
				_volume = nullptr;
				_modifier_id = 0;
			}
		} break;
		case NOTIFICATION_POST_ENTER_TREE: {
			update_volume();
		} break;
		case Node3D::NOTIFICATION_TRANSFORM_CHANGED:
		case Node3D::NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			update_volume();
		} break;
	}
}

#ifdef TOOLS_ENABLED

#if defined(ZN_GODOT)
PackedStringArray VoxelModifier::get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#elif defined(ZN_GODOT_EXTENSION)
PackedStringArray VoxelModifier::_get_configuration_warnings() const {
	PackedStringArray warnings;
	get_configuration_warnings(warnings);
	return warnings;
}
#endif

void VoxelModifier::get_configuration_warnings(PackedStringArray &warnings) const {
	if (_volume == nullptr) {
		warnings.append(ZN_TTR("The parent of this node must be of type {0}.")
								.format(varray(VoxelLodTerrain::get_class_static())));
	}
}

#endif

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

} // namespace zylann::voxel::gd
