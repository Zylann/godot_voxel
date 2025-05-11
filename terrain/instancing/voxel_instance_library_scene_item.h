#ifndef VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
#define VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H

#include "../../util/godot/classes/packed_scene.h"
#include "voxel_instance_library_item.h"

namespace zylann::voxel {

class VoxelInstanceLibrarySceneItem : public VoxelInstanceLibraryItem {
	GDCLASS(VoxelInstanceLibrarySceneItem, VoxelInstanceLibraryItem)
public:
	// Practical maximum to avoid using too much memory
	static constexpr unsigned int MAX_POOL_SIZE = 4096;

	~VoxelInstanceLibrarySceneItem();

	void set_scene(Ref<PackedScene> scene);
	Ref<PackedScene> get_scene() const;

	void set_pool_size(const int new_size);
	int get_pool_size() const;

	int get_pooled_instance_count() const;

	// Internal

	Node3D *create_instance();
	void free_instance(Node3D *node);

#ifdef TOOLS_ENABLED
	void get_configuration_warnings(PackedStringArray &warnings) const override;
#endif

private:
	void clear_pool(const unsigned int target_count);

private:
	static void _bind_methods();

	Ref<PackedScene> _scene;
	StdVector<Node3D *> _pool;
	unsigned int _pool_size = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_SCENE_ITEM_H
