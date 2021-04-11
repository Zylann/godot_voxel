#include "voxel_instance_library_item.h"
#include "voxel_instancer.h"

#include <core/core_string_names.h>
#include <scene/3d/collision_shape.h>
#include <scene/3d/mesh_instance.h>
#include <scene/3d/physics_body.h>

void VoxelInstanceLibraryItem::set_item_name(String name) {
	_name = name;
}

String VoxelInstanceLibraryItem::get_item_name() const {
	return _name;
}

void VoxelInstanceLibraryItem::set_lod_index(int lod) {
	ERR_FAIL_COND(lod < 0 || lod >= VoxelInstancer::MAX_LOD);
	if (_lod_index == lod) {
		return;
	}
	_lod_index = lod;
	notify_listeners(CHANGE_LOD_INDEX);
}

int VoxelInstanceLibraryItem::get_lod_index() const {
	return _lod_index;
}

void VoxelInstanceLibraryItem::set_generator(Ref<VoxelInstanceGenerator> generator) {
	if (_generator == generator) {
		return;
	}
	if (_generator.is_valid()) {
		_generator->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_generator_changed");
	}
	_generator = generator;
	if (_generator.is_valid()) {
		_generator->connect(CoreStringNames::get_singleton()->changed, this, "_on_generator_changed");
	}
	notify_listeners(CHANGE_GENERATOR);
}

Ref<VoxelInstanceGenerator> VoxelInstanceLibraryItem::get_generator() const {
	return _generator;
}

void VoxelInstanceLibraryItem::set_persistent(bool persistent) {
	_persistent = persistent;
}

bool VoxelInstanceLibraryItem::is_persistent() const {
	return _persistent;
}

void VoxelInstanceLibraryItem::set_mesh(Ref<Mesh> mesh, int mesh_lod_index) {
	ERR_FAIL_INDEX(mesh_lod_index, VoxelInstancer::MAX_LOD);
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
	ERR_FAIL_INDEX_V(mesh_lod_index, VoxelInstancer::MAX_LOD, Ref<Mesh>());
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

void VoxelInstanceLibraryItem::set_cast_shadows_setting(VisualServer::ShadowCastingSetting mode) {
	if (mode == _shadow_casting_setting) {
		return;
	}
	_shadow_casting_setting = mode;
	notify_listeners(CHANGE_VISUAL);
}

VisualServer::ShadowCastingSetting VoxelInstanceLibraryItem::get_cast_shadows_setting() const {
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

static VisualServer::ShadowCastingSetting node_to_visual_server_enum(GeometryInstance::ShadowCastingSetting v) {
	switch (v) {
		case GeometryInstance::SHADOW_CASTING_SETTING_OFF:
			return VisualServer::SHADOW_CASTING_SETTING_OFF;

		case GeometryInstance::SHADOW_CASTING_SETTING_ON:
			return VisualServer::SHADOW_CASTING_SETTING_ON;

		case GeometryInstance::SHADOW_CASTING_SETTING_DOUBLE_SIDED:
			return VisualServer::SHADOW_CASTING_SETTING_DOUBLE_SIDED;

		case GeometryInstance::SHADOW_CASTING_SETTING_SHADOWS_ONLY:
			return VisualServer::SHADOW_CASTING_SETTING_SHADOWS_ONLY;

		default:
			ERR_PRINT("Unknown ShadowCastingSetting value");
			return VisualServer::SHADOW_CASTING_SETTING_OFF;
	}
}

void VoxelInstanceLibraryItem::setup_from_template(Node *root) {
	_collision_shapes.clear();

	PhysicsBody *physics_body = Object::cast_to<PhysicsBody>(root);
	if (physics_body != nullptr) {
		_collision_layer = physics_body->get_collision_layer();
		_collision_mask = physics_body->get_collision_mask();
	}

	for (int i = 0; i < root->get_child_count(); ++i) {
		MeshInstance *mi = Object::cast_to<MeshInstance>(root->get_child(i));
		if (mi != nullptr) {
			_mesh_lods[0] = mi->get_mesh();
			_mesh_lod_count = 1;
			_material_override = mi->get_material_override();
			_shadow_casting_setting = node_to_visual_server_enum(mi->get_cast_shadows_setting());
		}

		if (physics_body != nullptr) {
			CollisionShape *cs = Object::cast_to<CollisionShape>(physics_body->get_child(i));

			if (cs != nullptr) {
				CollisionShapeInfo info;
				info.shape = cs->get_shape();
				info.transform = cs->get_transform();

				_collision_shapes.push_back(info);
			}
		}
	}

	notify_listeners(CHANGE_VISUAL);
}

void VoxelInstanceLibraryItem::add_listener(IListener *listener, int id) {
	ListenerSlot slot;
	slot.listener = listener;
	slot.id = id;
	ERR_FAIL_COND(_listeners.find(slot) != -1);
	_listeners.push_back(slot);
}

void VoxelInstanceLibraryItem::remove_listener(IListener *listener, int id) {
	ListenerSlot slot;
	slot.listener = listener;
	slot.id = id;
	int i = _listeners.find(slot);
	ERR_FAIL_COND(i == -1);
	_listeners.remove(i);
}

void VoxelInstanceLibraryItem::notify_listeners(ChangeType change) {
	for (int i = 0; i < _listeners.size(); ++i) {
		ListenerSlot &slot = _listeners.write[i];
		slot.listener->on_library_item_changed(slot.id, change);
	}
}

void VoxelInstanceLibraryItem::_on_generator_changed() {
	notify_listeners(CHANGE_GENERATOR);
}

void VoxelInstanceLibraryItem::_b_set_collision_shapes(Array shape_infos) {
	ERR_FAIL_COND(shape_infos.size() % 2 != 0);

	_collision_shapes.clear();

	for (int i = 0; i < shape_infos.size(); i += 2) {
		CollisionShapeInfo info;
		info.transform = shape_infos[i];
		info.shape = shape_infos[i + 1];

		ERR_FAIL_COND(info.shape.is_null());

		_collision_shapes.push_back(info);
	}
}

Array VoxelInstanceLibraryItem::_b_get_collision_shapes() const {
	Array infos;
	for (int i = 0; i < _collision_shapes.size(); ++i) {
		const CollisionShapeInfo &info = _collision_shapes[i];
		infos.push_back(info.shape);
		infos.push_back(info.transform);
	}
	return infos;
}

void VoxelInstanceLibraryItem::_bind_methods() {
	// Can't be just "set_name" because Resource already defines that, despite being for a `resource_name` property
	ClassDB::bind_method(D_METHOD("set_item_name", "name"), &VoxelInstanceLibraryItem::set_item_name);
	ClassDB::bind_method(D_METHOD("get_item_name"), &VoxelInstanceLibraryItem::get_item_name);

	ClassDB::bind_method(D_METHOD("set_lod_index", "lod"), &VoxelInstanceLibraryItem::set_lod_index);
	ClassDB::bind_method(D_METHOD("get_lod_index"), &VoxelInstanceLibraryItem::get_lod_index);

	ClassDB::bind_method(D_METHOD("set_generator", "generator"), &VoxelInstanceLibraryItem::set_generator);
	ClassDB::bind_method(D_METHOD("get_generator"), &VoxelInstanceLibraryItem::get_generator);

	ClassDB::bind_method(D_METHOD("set_persistent", "persistent"), &VoxelInstanceLibraryItem::set_persistent);
	ClassDB::bind_method(D_METHOD("is_persistent"), &VoxelInstanceLibraryItem::is_persistent);

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

	ClassDB::bind_method(D_METHOD("_on_generator_changed"), &VoxelInstanceLibraryItem::_on_generator_changed);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_item_name", "get_item_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_index", PROPERTY_HINT_RANGE, "0,8,1"),
			"set_lod_index", "get_lod_index");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "generator", PROPERTY_HINT_RESOURCE_TYPE, "VoxelInstanceGenerator"),
			"set_generator", "get_generator");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "persistent"), "set_persistent", "is_persistent");

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

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows_setting", "get_cast_shadows_setting");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS),
			"set_collision_mask", "get_collision_mask");

	BIND_CONSTANT(MAX_MESH_LODS);
}
