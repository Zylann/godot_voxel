#include "voxel_modifier_gd.h"
#include "../../util/godot/core/array.h"
#include "../../util/io/log.h"
#include "../../util/math/conv.h"
#include "../voxel_modifier_sdf.h"

#ifdef TOOLS_ENABLED
#include "../../util/godot/core/packed_arrays.h"
#endif

namespace zylann::voxel::godot {

VoxelModifier::VoxelModifier() {
	set_notify_transform(true);
}

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

// Essentially, the transform this modifier would have were it a direct
// child of the terrain.
Transform3D VoxelModifier::get_terrain_local_transform() const {
	ERR_FAIL_COND_V(_volume == nullptr, Transform3D());
	return _volume->get_global_transform().affine_inverse() * get_global_transform();
}

VoxelLodTerrain *VoxelModifier::get_ancestor_volume() const {
	Node *parent = get_parent();
	while (parent != nullptr) {
		VoxelLodTerrain *volume = Object::cast_to<VoxelLodTerrain>(parent);
		if (volume != nullptr) {
			return volume;
		}
		parent = parent->get_parent();
	}
	return nullptr;
}

void VoxelModifier::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ZN_ASSERT_RETURN(_volume == nullptr);

			_volume = get_ancestor_volume();
			if (_volume != nullptr) {
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

				Transform3D terrain_local = get_terrain_local_transform();
				modifier->set_transform(terrain_local);
				_last_tl_transform = terrain_local;
				_modifier_id = id;
				// TODO Optimize: on loading of a scene, this could be very bad for performance because there could be,
				// a lot of modifiers on the map, but there is no distinction possible in Godot at the moment...
				post_edit_modifier(*_volume, modifier->get_aabb());
			}

			update_configuration_warnings();
		} break;

		case NOTIFICATION_EXIT_TREE: {
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

		case Node3D::NOTIFICATION_TRANSFORM_CHANGED: {
			if (_volume == nullptr || !is_inside_tree()) {
				return;
			}

			// Only trigger update if the transform has actually changed in terrain-local space.
			const Transform3D terrain_local = get_terrain_local_transform();
			if (terrain_local.is_equal_approx(_last_tl_transform)) {
				return;
			}
			_last_tl_transform = terrain_local;

			VoxelData &voxel_data = _volume->get_storage();
			VoxelModifierStack &modifiers = voxel_data.get_modifiers();
			zylann::voxel::VoxelModifier *modifier = modifiers.get_modifier(_modifier_id);
			ZN_ASSERT_RETURN(modifier != nullptr);

			
			const AABB prev_aabb = modifier->get_aabb();
			modifier->set_transform(terrain_local);
			const AABB aabb = modifier->get_aabb();
			post_edit_modifier(*_volume, prev_aabb);
			post_edit_modifier(*_volume, aabb);
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
		warnings.append(ZN_TTR("An ancestor of this node must be of type {0}.")
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

} // namespace zylann::voxel::godot
