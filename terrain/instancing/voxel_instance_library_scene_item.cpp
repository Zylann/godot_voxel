#include "voxel_instance_library_scene_item.h"

void VoxelInstanceLibrarySceneItem::set_scene(Ref<PackedScene> scene) {
	if (scene != _scene) {
		_scene = scene;
		notify_listeners(CHANGE_SCENE);
	}
}

Ref<PackedScene> VoxelInstanceLibrarySceneItem::get_scene() const {
	return _scene;
}

void VoxelInstanceLibrarySceneItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_scene", "scene"), &VoxelInstanceLibrarySceneItem::set_scene);
	ClassDB::bind_method(D_METHOD("get_scene"), &VoxelInstanceLibrarySceneItem::get_scene);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"),
			"set_scene", "get_scene");
}
