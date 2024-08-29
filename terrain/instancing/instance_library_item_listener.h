#ifndef VOXEL_INSTANCE_LIBRARY_ITEM_LISTENER_H
#define VOXEL_INSTANCE_LIBRARY_ITEM_LISTENER_H

namespace zylann::voxel {

class VoxelInstanceLibraryItem;

class IInstanceLibraryItemListener {
public:
	enum ItemChangeType { //
		ITEM_CHANGE_VISUAL,
		ITEM_CHANGE_SCENE
	};

	virtual ~IInstanceLibraryItemListener() {}
	virtual void on_library_item_changed(VoxelInstanceLibraryItem *item, ItemChangeType change) = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_ITEM_LISTENER_H
