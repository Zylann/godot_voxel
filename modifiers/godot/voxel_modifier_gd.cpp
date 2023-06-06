#include "voxel_modifier_gd.h"
#include "../../util/log.h"
#include "../../util/math/conv.h"
#include "../voxel_modifier_sdf.h"

namespace zylann::voxel::gd {

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

void VoxelModifier::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PARENTED: {
			Node *parent = get_parent();
			ZN_ASSERT_RETURN(parent != nullptr);
			ZN_ASSERT_RETURN(_volume == nullptr);
			VoxelLodTerrain *volume = Object::cast_to<VoxelLodTerrain>(parent);
			_volume = volume;

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

				modifier->set_transform(get_transform());
				_modifier_id = id;
				// TODO Optimize: on loading of a scene, this could be very bad for performance because there could be,
				// a lot of modifiers on the map, but there is no distinction possible in Godot at the moment...
				post_edit_modifier(*_volume, modifier->get_aabb());
			}
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

		case Node3D::NOTIFICATION_LOCAL_TRANSFORM_CHANGED: {
			if (_volume != nullptr && is_inside_tree()) {
				VoxelData &voxel_data = _volume->get_storage();
				VoxelModifierStack &modifiers = voxel_data.get_modifiers();
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

} // namespace zylann::voxel::gd
