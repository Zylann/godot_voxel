#include "voxel_instance_library_multimesh_item.h"
#include "../../constants/voxel_string_names.h"
#include "../../util/godot/classes/collision_shape_3d.h"
#include "../../util/godot/classes/mesh_instance_3d.h"
#include "../../util/godot/classes/physics_body_3d.h"
#include "voxel_instancer.h"

namespace zylann::voxel {

const char *VoxelInstanceLibraryMultiMeshItem::MANUAL_SETTINGS_GROUP_NAME = "Manual settings";
const char *VoxelInstanceLibraryMultiMeshItem::SCENE_SETTINGS_GROUP_NAME = "Scene properties";

static Array serialize_collision_shape_infos(
		const std::vector<VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo> &infos) {
	Array a;
	for (unsigned int i = 0; i < infos.size(); ++i) {
		const VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo &info = infos[i];
		ERR_FAIL_COND_V(info.shape.is_null(), Array());
		// TODO Shape might or might not be shared, could have odd side-effects,
		// but not sure how to properly fix these edge cases without convoluted code
		a.push_back(info.shape);
		a.push_back(info.transform);
	}
	return a;
}

static bool deserialize_collision_shape_infos(
		Array a, std::vector<VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo> &out_infos) {
	ERR_FAIL_COND_V(a.size() % 2 != 0, false);

	for (int i = 0; i < a.size(); i += 2) {
		VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo info;
		info.shape = a[i];
		info.transform = a[i + 1];

		ERR_FAIL_COND_V(info.shape.is_null(), false);

		out_infos.push_back(info);
	}

	return false;
}

void VoxelInstanceLibraryMultiMeshItem::set_mesh(Ref<Mesh> mesh, int mesh_lod_index) {
	Settings &settings = _manual_settings;
	ERR_FAIL_INDEX(mesh_lod_index, static_cast<int>(settings.mesh_lods.size()));
	if (settings.mesh_lods[mesh_lod_index] == mesh) {
		return;
	}
	settings.mesh_lods[mesh_lod_index] = mesh;

	// Update count
	unsigned int count = settings.mesh_lods.size();
	for (unsigned int i = settings.mesh_lods.size() - 1; i > 0; --i) {
		if (settings.mesh_lods[i].is_valid()) {
			break;
		}
		--count;
	}
	settings.mesh_lod_count = count;

	notify_listeners(CHANGE_VISUAL);
}

int VoxelInstanceLibraryMultiMeshItem::get_mesh_lod_count() const {
	return _manual_settings.mesh_lod_count;
}

Ref<Mesh> VoxelInstanceLibraryMultiMeshItem::get_mesh(int mesh_lod_index) const {
	const Settings &settings = _manual_settings;
	ERR_FAIL_INDEX_V(mesh_lod_index, static_cast<int>(settings.mesh_lods.size()), Ref<Mesh>());
	return settings.mesh_lods[mesh_lod_index];
}

void VoxelInstanceLibraryMultiMeshItem::set_material_override(Ref<Material> material) {
	Settings &settings = _manual_settings;
	if (material == settings.material_override) {
		return;
	}
	settings.material_override = material;
	notify_listeners(CHANGE_VISUAL);
}

Ref<Material> VoxelInstanceLibraryMultiMeshItem::get_material_override() const {
	const Settings &settings = _manual_settings;
	return settings.material_override;
}

void VoxelInstanceLibraryMultiMeshItem::set_cast_shadows_setting(RenderingServer::ShadowCastingSetting mode) {
	Settings &settings = _manual_settings;
	if (mode == settings.shadow_casting_setting) {
		return;
	}
	settings.shadow_casting_setting = mode;
	notify_listeners(CHANGE_VISUAL);
}

RenderingServer::ShadowCastingSetting VoxelInstanceLibraryMultiMeshItem::get_cast_shadows_setting() const {
	const Settings &settings = _manual_settings;
	return settings.shadow_casting_setting;
}

void VoxelInstanceLibraryMultiMeshItem::set_collision_layer(int collision_layer) {
	Settings &settings = _manual_settings;
	settings.collision_layer = collision_layer;
}

int VoxelInstanceLibraryMultiMeshItem::get_collision_layer() const {
	const Settings &settings = _manual_settings;
	return settings.collision_layer;
}

void VoxelInstanceLibraryMultiMeshItem::set_collision_mask(int collision_mask) {
	Settings &settings = _manual_settings;
	settings.collision_mask = collision_mask;
}

int VoxelInstanceLibraryMultiMeshItem::get_collision_mask() const {
	const Settings &settings = _manual_settings;
	return settings.collision_mask;
}

namespace {
static const char *CAST_SHADOW_ENUM_NAMES = "Off,On,Double-Sided,Shadows Only";
}

void VoxelInstanceLibraryMultiMeshItem::_get_property_list(List<PropertyInfo> *p_list) const {
	if (_scene.is_valid()) {
		// This is only so we have a preview of conversion results.

		p_list->push_back(PropertyInfo(
				Variant::NIL, SCENE_SETTINGS_GROUP_NAME, PROPERTY_HINT_NONE, "scene_", PROPERTY_USAGE_GROUP));

		p_list->push_back(PropertyInfo(Variant::OBJECT, "scene_mesh", PROPERTY_HINT_RESOURCE_TYPE,
				Mesh::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::OBJECT, "scene_mesh_lod1", PROPERTY_HINT_RESOURCE_TYPE,
				Mesh::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::OBJECT, "scene_mesh_lod2", PROPERTY_HINT_RESOURCE_TYPE,
				Mesh::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::OBJECT, "scene_mesh_lod3", PROPERTY_HINT_RESOURCE_TYPE,
				Mesh::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));

		p_list->push_back(PropertyInfo(Variant::OBJECT, "scene_material_override", PROPERTY_HINT_RESOURCE_TYPE,
				Material::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::INT, "scene_cast_shadow", PROPERTY_HINT_ENUM, CAST_SHADOW_ENUM_NAMES,
				PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::INT, "scene_collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS, "",
				PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::INT, "scene_collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS, "",
				PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
		p_list->push_back(PropertyInfo(Variant::ARRAY, "scene_collision_shapes", PROPERTY_HINT_NONE, "",
				PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY));
	}
}

bool VoxelInstanceLibraryMultiMeshItem::_get(const StringName &p_name, Variant &r_ret) const {
	if (_scene.is_valid()) {
		// TODO GDX: GDExtension does not have `StringName::operator==(const char*)`
		const String property_name = p_name;

		if (property_name == "scene_mesh") {
			r_ret = _scene_settings.mesh_lods[0];
			return true;
		}
		if (property_name == "scene_mesh_lod1") {
			r_ret = _scene_settings.mesh_lods[1];
			return true;
		}
		if (property_name == "scene_mesh_lod2") {
			r_ret = _scene_settings.mesh_lods[2];
			return true;
		}
		if (property_name == "scene_mesh_lod3") {
			r_ret = _scene_settings.mesh_lods[3];
			return true;
		}
		if (property_name == "scene_material_override") {
			r_ret = _scene_settings.material_override;
			return true;
		}
		if (property_name == "scene_cast_shadow") {
			r_ret = _scene_settings.shadow_casting_setting;
			return true;
		}
		if (property_name == "scene_collision_layer") {
			r_ret = _scene_settings.collision_layer;
			return true;
		}
		if (property_name == "scene_collision_mask") {
			r_ret = _scene_settings.collision_mask;
			return true;
		}
		if (property_name == "scene_collision_shapes") {
			r_ret = serialize_collision_shape_infos(_scene_settings.collision_shapes);
			return true;
		}
	}
	return false;
}

// bool VoxelInstanceLibraryMultiMeshItem::_set(const StringName &p_name, const Variant &p_value) {
// 	return false;
// }

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

static bool setup_from_template(Node *root, VoxelInstanceLibraryMultiMeshItem::Settings &settings) {
	struct L {
		static unsigned int get_lod_index_from_name(const String &name) {
			if (name.ends_with("LOD0")) {
				return 0;
			}
			if (name.ends_with("LOD1")) {
				return 1;
			}
			if (name.ends_with("LOD2")) {
				return 2;
			}
			if (name.ends_with("LOD3")) {
				return 3;
			}
			return 0;
		}
	};

	ERR_FAIL_COND_V(root == nullptr, false);

	settings.collision_shapes.clear();

	PhysicsBody3D *physics_body = Object::cast_to<PhysicsBody3D>(root);
	if (physics_body != nullptr) {
		settings.collision_layer = physics_body->get_collision_layer();
		settings.collision_mask = physics_body->get_collision_mask();
	}

	for (int i = 0; i < root->get_child_count(); ++i) {
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(root->get_child(i));
		if (mi != nullptr) {
			const unsigned int lod_index = L::get_lod_index_from_name(mi->get_name());
			settings.mesh_lods[lod_index] = mi->get_mesh();
			settings.mesh_lod_count = math::max(lod_index + 1, settings.mesh_lod_count);
			settings.material_override = mi->get_material_override();
			settings.shadow_casting_setting = node_to_visual_server_enum(mi->get_cast_shadows_setting());
		}

		if (physics_body != nullptr) {
			CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(physics_body->get_child(i));

			if (cs != nullptr) {
				VoxelInstanceLibraryMultiMeshItem::CollisionShapeInfo info;
				info.shape = cs->get_shape();
				info.transform = cs->get_transform();

				settings.collision_shapes.push_back(info);
			}
		}
	}

	return true;
}

#if defined(ZN_GODOT)
void VoxelInstanceLibraryMultiMeshItem::setup_from_template(Node *root) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelInstanceLibraryMultiMeshItem::setup_from_template(Object *root_o) {
	Node *root = Object::cast_to<Node>(root_o);
#endif
	ERR_FAIL_COND(!zylann::voxel::setup_from_template(root, _manual_settings));
	notify_listeners(CHANGE_VISUAL);
}

void VoxelInstanceLibraryMultiMeshItem::set_scene(Ref<PackedScene> scene) {
	if (_scene == scene) {
		return;
	}
	_scene = scene;
	if (_scene.is_valid()) {
		Node *root = _scene->instantiate();
		ERR_FAIL_COND(root == nullptr);
		ERR_FAIL_COND(!zylann::voxel::setup_from_template(root, _scene_settings));
		memdelete(root);
	}
	notify_listeners(CHANGE_VISUAL);
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif
}

Ref<PackedScene> VoxelInstanceLibraryMultiMeshItem::get_scene() const {
	return _scene;
}

const VoxelInstanceLibraryMultiMeshItem::Settings &VoxelInstanceLibraryMultiMeshItem::get_multimesh_settings() const {
	if (_scene.is_valid()) {
		return _scene_settings;
	} else {
		return _manual_settings;
	}
}

Array VoxelInstanceLibraryMultiMeshItem::serialize_multimesh_item_properties() const {
	const Settings &settings = _manual_settings;
	Array a;
	for (unsigned int i = 0; i < settings.mesh_lods.size(); ++i) {
		a.push_back(settings.mesh_lods[i]);
	}
	a.push_back(settings.mesh_lod_count);
	a.push_back(settings.material_override);
	a.push_back(settings.shadow_casting_setting);
	a.push_back(settings.collision_layer);
	a.push_back(settings.collision_mask);
	a.push_back(serialize_collision_shape_infos(settings.collision_shapes));
	return a;
}

void VoxelInstanceLibraryMultiMeshItem::deserialize_multimesh_item_properties(Array a) {
	Settings &settings = _manual_settings;
	ERR_FAIL_COND(a.size() != int(settings.mesh_lods.size()) + 6);
	int ai = 0;
	for (unsigned int i = 0; i < settings.mesh_lods.size(); ++i) {
		settings.mesh_lods[i] = a[ai++];
	}
	settings.mesh_lod_count = a[ai++];
	settings.material_override = a[ai++];
	settings.shadow_casting_setting = RenderingServer::ShadowCastingSetting(int(a[ai++])); // ugh...
	settings.collision_layer = a[ai++];
	settings.collision_mask = a[ai++];
	settings.collision_shapes.clear();
	deserialize_collision_shape_infos(a[ai++], settings.collision_shapes);
	notify_listeners(CHANGE_VISUAL);
}

void VoxelInstanceLibraryMultiMeshItem::_b_set_collision_shapes(Array shape_infos) {
	Settings &settings = _manual_settings;
	settings.collision_shapes.clear();
	deserialize_collision_shape_infos(shape_infos, settings.collision_shapes);
}

Array VoxelInstanceLibraryMultiMeshItem::_b_get_collision_shapes() const {
	const Settings &settings = _manual_settings;
	return serialize_collision_shape_infos(settings.collision_shapes);
}

void VoxelInstanceLibraryMultiMeshItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh", "mesh_lod_index"), &VoxelInstanceLibraryMultiMeshItem::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh", "mesh_lod_index"), &VoxelInstanceLibraryMultiMeshItem::get_mesh);

	ClassDB::bind_method(D_METHOD("_set_mesh_lod0", "mesh"), &VoxelInstanceLibraryMultiMeshItem::_b_set_mesh_lod0);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod1", "mesh"), &VoxelInstanceLibraryMultiMeshItem::_b_set_mesh_lod1);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod2", "mesh"), &VoxelInstanceLibraryMultiMeshItem::_b_set_mesh_lod2);
	ClassDB::bind_method(D_METHOD("_set_mesh_lod3", "mesh"), &VoxelInstanceLibraryMultiMeshItem::_b_set_mesh_lod3);

	ClassDB::bind_method(D_METHOD("_get_mesh_lod0"), &VoxelInstanceLibraryMultiMeshItem::_b_get_mesh_lod0);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod1"), &VoxelInstanceLibraryMultiMeshItem::_b_get_mesh_lod1);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod2"), &VoxelInstanceLibraryMultiMeshItem::_b_get_mesh_lod2);
	ClassDB::bind_method(D_METHOD("_get_mesh_lod3"), &VoxelInstanceLibraryMultiMeshItem::_b_get_mesh_lod3);

	ClassDB::bind_method(
			D_METHOD("set_material_override", "material"), &VoxelInstanceLibraryMultiMeshItem::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &VoxelInstanceLibraryMultiMeshItem::get_material_override);

	ClassDB::bind_method(
			D_METHOD("set_cast_shadows_setting", "mode"), &VoxelInstanceLibraryMultiMeshItem::set_cast_shadows_setting);
	ClassDB::bind_method(
			D_METHOD("get_cast_shadows_setting"), &VoxelInstanceLibraryMultiMeshItem::get_cast_shadows_setting);

	ClassDB::bind_method(D_METHOD("set_collision_layer", "collision_layer"),
			&VoxelInstanceLibraryMultiMeshItem::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &VoxelInstanceLibraryMultiMeshItem::get_collision_layer);

	ClassDB::bind_method(
			D_METHOD("set_collision_mask", "collision_mask"), &VoxelInstanceLibraryMultiMeshItem::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VoxelInstanceLibraryMultiMeshItem::get_collision_mask);

	ClassDB::bind_method(D_METHOD("set_collision_shapes", "shape_infos"),
			&VoxelInstanceLibraryMultiMeshItem::_b_set_collision_shapes);
	ClassDB::bind_method(D_METHOD("get_collision_shapes"), &VoxelInstanceLibraryMultiMeshItem::_b_get_collision_shapes);

	ClassDB::bind_method(
			D_METHOD("setup_from_template", "node"), &VoxelInstanceLibraryMultiMeshItem::setup_from_template);

	ClassDB::bind_method(D_METHOD("get_scene"), &VoxelInstanceLibraryMultiMeshItem::get_scene);
	ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VoxelInstanceLibraryMultiMeshItem::set_scene);

	// Used in editor only
	ClassDB::bind_method(D_METHOD("_deserialize_multimesh_item_properties", "props"),
			&VoxelInstanceLibraryMultiMeshItem::deserialize_multimesh_item_properties);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, PackedScene::get_class_static()),
			"set_scene", "get_scene");

	ADD_GROUP(MANUAL_SETTINGS_GROUP_NAME, "");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"_set_mesh_lod0", "_get_mesh_lod0");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod1", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"_set_mesh_lod1", "_get_mesh_lod1");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod2", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"_set_mesh_lod2", "_get_mesh_lod2");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh_lod3", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()),
			"_set_mesh_lod3", "_get_mesh_lod3");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE,
						 // TODO Disallow CanvasItemMaterial?
						 Material::get_class_static()),
			"set_material_override", "get_material_override");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadow", PROPERTY_HINT_ENUM, CAST_SHADOW_ENUM_NAMES),
			"set_cast_shadows_setting", "get_cast_shadows_setting");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer",
			"get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask",
			"get_collision_mask");

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "collision_shapes"), "set_collision_shapes", "get_collision_shapes");

	BIND_CONSTANT(MAX_MESH_LODS);
}

} // namespace zylann::voxel
