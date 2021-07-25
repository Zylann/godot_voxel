#ifndef VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H

#include "voxel_instance_library_item_base.h"

#include <scene/resources/packed_scene.h>

class VoxelInstanceLibrarySceneItem : public VoxelInstanceLibraryItemBase {
	GDCLASS(VoxelInstanceLibrarySceneItem, VoxelInstanceLibraryItemBase)
public:
	void set_scene(Ref<PackedScene> scene);
	Ref<PackedScene> get_scene() const;

private:
	static void _bind_methods();

	Ref<PackedScene> _scene;
};

#endif // VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
