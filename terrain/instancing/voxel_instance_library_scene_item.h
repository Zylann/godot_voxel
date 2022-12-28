#ifndef VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H

#include "../../util/godot/classes/packed_scene.h"
#include "voxel_instance_library_item.h"

namespace zylann::voxel {

class VoxelInstanceLibrarySceneItem : public VoxelInstanceLibraryItem {
	GDCLASS(VoxelInstanceLibrarySceneItem, VoxelInstanceLibraryItem)
public:
	void set_scene(Ref<PackedScene> scene);
	Ref<PackedScene> get_scene() const;

private:
	static void _bind_methods();

	Ref<PackedScene> _scene;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
