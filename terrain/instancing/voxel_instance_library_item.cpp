#include "voxel_instance_library_item.h"
#include "voxel_instancer.h"

#include <core/core_string_names.h>
#include <scene/3d/collision_shape_3d.h>
#include <scene/3d/mesh_instance_3d.h>
#include <scene/3d/physics_body_3d.h>

void VoxelInstanceLibraryItem::set_mesh(Ref<Mesh> mesh, int mesh_lod_index) {
	ERR_FAIL_INDEX(mesh_lod_index, static_cast<int>(_mesh_lods.size()));
	if (_mesh_lods[mesh_lod_index] == mesh) {
		return;
	}
	_mesh_lods[mesh_lod_index] = mesh;

	// Update count
	unsigned int count = _mesh_lods.size();
	for (unsigned int i = _mesh_lods.size() - 1; i > 0; --i) {
		if (_mesh_lods[i].is_valid()) {
			break;
		}
		--count;
	}
	_mesh_lod_count = count;

	notify_listeners(CHANGE_VISUAL);
}

int VoxelInstanceLibraryItem::get_mesh_lod_count() const {
	return _mesh_lod_count;
}

Ref<Mesh> VoxelInstanceLibraryItem::get_mesh(int mesh_lod_index) const {
	ERR_FAIL_INDEX_V(mesh_lod_index, static_cast<int>(_mesh_lods.size()), Ref<Mesh>());
	return _mesh_lods[mesh_lod_index];
}

void VoxelInstanceLibraryItem::set_material_override(Ref<Material> material) {
	if (material == _material_override) {
		return;
	}
	_material_override = material;
	notify_listeners(CHANGE_VISUAL);
}

Ref<Material> VoxelInstanceLibraryItem::get_material_override() const {
	return _material_override;
}

void VoxelInstanceLibraryItem::set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode) {
	if (mode == _shadow_casting_setting) {
		return;
	}
	_shadow_casting_setting = mode;
	notify_listeners(CHANGE_VISUAL);
}

RenderingServer::ShadowCastingSetting VoxelInstanceLibraryItem::get_cast_shadows_setting() const {
	return _shadow_casting_setting;
}

void VoxelInstanceLibraryItem::set_collision_layer(int collision_layer) {
	_collision_layer = collision_layer;
}

int VoxelInstanceLibraryItem::get_collision_layer() const {
	return _collision_layer;
}

void VoxelInstanceLibraryItem::set_collision_mask(int collision_mask) {
	_collision_mask = collision_mask;
}

int VoxelInstanceLibraryItem::get_collision_mask() const {
	return _collision_mask;
}

static RenderingServer::ShadowCastingSetting node_to_visual_server_enum(GeometryInstance3D::ShadowCastingSetting v) {
	switch (v) {
		case GeometryInstance3D::SHADOW_CASTING_SETTING_OFF:
			return RenderingServer::SHADOW_CASTING_SETTING_OFF;

		case GeometryInstance3D::SHADOW_CASTING_SETTING_ON:
			return RenderingServer::SHADOW_CASTING_SETTING_ON;

		case GeometryInstance3D::SHADOW_CASTING_SETTING_DOUBLE_SIDED:
			return RenderingServer::SHADOW_CASTING_SETTING_DOUBLE_SIDED;

		case GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY:
			return RenderingServer::SHADOW_CASTING_SETTING_SHADOWS_ONLY;

		default:
			ERR_PRINT("Unknown ShadowCastingSetting value");
			return RenderingServer::SHADOW_CASTING_SETTING_OFF;
	}
}

void VoxelInstanceLibraryItem::setup_from_template(Node *root) {
	ERR_FAIL_COND(root == nullptr);

	_collision_shapes.clear();

	PhysicsBody3D *physics_body = Object::cast_to<PhysicsBody3D>(root);
	if (physics_body != nullptr) {
		_collision_layer = physics_body->get_collision_layer();
		_collision_mask = physics_body->get_collision_mask();
	}

	for (int i = 0; i < root->get_child_count(); ++i) {
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(root->get_child(i));
		if (mi != nullptr) {
			_mesh_lods[0] = mi->get_mesh();
			_mesh_lod_count = 1;
			_material_override = mi->get_material_override();
			_shadow_casting_setting = node_to_visual_server_enum(mi->get_cast_shadows_setting());
		}

		if (physics_body != nullptr) {
			CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(physics_body->get_child(i));

			if (cs != nullptr) {
				CollisionShape3DInfo info;
				info.shape = cs->get_shape();
				info.transform = cs->get_transform();

				_collision_shapes.push_back(info);
			}
		}
	}

	notify_listeners(CHANGE_VISUAL);
}

static Array serialize_collision_shape_infos(Vector<VoxelInstanceLibraryItem::CollisionShape3DInfo> infos) {
	Array a;
	for (int i = 0; i < infos.size(); ++i) {
		const VoxelInstanceLibraryItem::CollisionShape3DInfo &info = infos[i];
		ERR_FAIL_COND_V(info.shape.is_null(), Array());
		// TODO Shape might or might not be shared, could have odd side-effects,
		// but not sure how to properly fix these edge cases without convoluted code
		a.push_back(info.shape);
		a.push_back(info.transform);
	}
	return a;
}

static Vector<VoxelInstanceLibraryItem::CollisionShape3DInfo> deserialize_collision_shape_infos(Array a) {
	Vector<VoxelInstanceLibraryItem::CollisionShape3DInfo> infos;
	ERR_FAIL_COND_V(a.size() % 2 != 0, infos);

	for (int i = 0; i < a.size(); i += 2) {
		VoxelInstanceLibraryItem::CollisionShape3DInfo info;
		info.shape = a[i];
		info.transform = a[i + 1];

		ERR_FAIL_COND_V(info.shape.is_null(), Vector<VoxelInstanceLibraryItem::CollisionShape3DInfo>());

		infos.push_back(info);
	}

	return infos;
}

// This is used to support undo/redo with the "setup from scene" feature
Array VoxelInstanceLibraryItem::serialize_multimesh_item_properties() const {
	Array a;
	for (unsigned int i = 0; i < _mesh_lods.size(); ++i) {
		a.push_back(_mesh_lods[i]);
	}
	a.push_back(_mesh_lod_count);
	a.push_back(_material_override);
	a.push_back(_shadow_casting_setting);
	a.push_back(_collision_layer);
	a.push_back(_collision_mask);
	a.push_back(serialize_collision_shape_infos(_collision_shapes));
	return a;
}

void VoxelInstanceLibraryItem::deserialize_multimesh_item_properties(Array a) {
	ERR_FAIL_COND(a.size() != int(_mesh_lods.size()) + 6);
	int ai = 0;
	for (unsigned int i = 0; i < _mesh_lods.size(); ++i) {
		_mesh_lods[i] = a[ai++];
	}
	_mesh_lod_count = a[ai++];
	_material_override = a[ai++];
	_shadow_casting_setting = RenderingServer::ShadowCastingSetting(int(a[ai++])); // ugh...
	_collision_layer = a[ai++];
	_collision_mask = a[ai++];
	_collision_shapes = deserialize_collision_shape_infos(a[ai++]);
	notify_listeners(CHANGE_VISUAL);
}

void VoxelInstanceLibraryItem::_b_set_collision_shapes(Array shape_infos) {
	_collision_shapes = deserialize_collision_shape_infos(shape_infos);
}

Array VoxelInstanceLibraryItem::_b_get_collision_shapes() const {
	return serialize_collision_shape_infos(_collision_shapes);
}

void VoxelInstanceLibraryItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh", "mesh_lod_index"), &VoxelInstanceLibraryItem::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh", "mesh_lod_index"), &VoxelInstanceLibraryItem::get_mesh);

	ClassDB::bind_method(D_METHOD("_set_mesh_lod0", "mesh"), &VoxelInstanceLibraryItem::_b_set_mesh_lod0);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod1", "mesh"), &VoxelInstanceLibraryItem::_b_set_mesh_lod1);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod2", "mesh"), &VoxelInstanceLibraryItem::_b_set_mesh_lod2);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod3", "mesh"), &VoxelInstanceLibraryItem::_b_set_mesh_lod3);

	ClassDB::bind_method(D_METHOD("_get_mesh_lod0"), &VoxelInstanceLibraryItem::_b_get_mesh_lod0);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod1"), &VoxelInstanceLibraryItem::_b_get_mesh_lod1);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod2"), &VoxelInstanceLibraryItem::_b_get_mesh_lod2);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod3"), &VoxelInstanceLibraryItem::_b_get_mesh_lod3);

	ClassDB::bind_method(D_METHOD("set_material_override", "material"),
			&VoxelInstanceLibraryItem::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &VoxelInstanceLibraryItem::get_material_override);

	ClassDB::bind_method(D_METHOD("set_cast_shadows_setting", "mode"),
			&VoxelInstanceLibraryItem::set_cast_shadows_setting);
	ClassDB::bind_method(D_METHOD("get_cast_shadows_setting"), &VoxelInstanceLibraryItem::get_cast_shadows_setting);

	ClassDB::bind_method(D_METHOD("set_collision_layer", "collision_layer"),
			&VoxelInstanceLibraryItem::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &VoxelInstanceLibraryItem::get_collision_layer);

	ClassDB::bind_method(D_METHOD("set_collision_mask", "collision_mask"),
			&VoxelInstanceLibraryItem::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelInstanceLibraryItem::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_shapes", "shape_infos"),
			&VoxelInstanceLibraryItem::_b_set_collision_shapes);
	ClassDB::bind_method(D_METHOD("get_collision_shapes"), &VoxelInstanceLibraryItem::_b_get_collision_shapes);

	ClassDB::bind_method(D_METHOD("setup_from_template", "node"), &VoxelInstanceLibraryItem::setup_from_template);

	// Used in editor only
	ClassDB::bind_method(D_METHOD("_deserialize_multimesh_item_properties", "props"),
			&VoxelInstanceLibraryItem::deserialize_multimesh_item_properties);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"),
			"_set_mesh_lod0", "_get_mesh_lod0");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod1", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"),
			"_set_mesh_lod1", "_get_mesh_lod1");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod2", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"),
			"_set_mesh_lod2", "_get_mesh_lod2");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod3", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"),
			"_set_mesh_lod3", "_get_mesh_lod3");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "Material"),
			"set_material_override", "get_material_override");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"),
			"set_cast_shadows_setting", "get_cast_shadows_setting");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_mask", "get_collision_mask");

	BIND_CONSTANT(MAX_MESH_LODS);
}
