#ifndef VOXEL_INSTANCE_LIBRARY_LISTENER_H
#define VOXEL_INSTANCE_LIBRARY_LISTENER_H

namespace zylann::voxel {

class VoxelInstanceEmitter;

class IInstanceLibraryListener {
public:
	virtual ~IInstanceLibraryListener() {}

	enum ItemChange { ITEM_CHANGE_VISUAL, ITEM_CHANGE_SCENE };

	virtual void on_library_item_registered(const int layer_id) = 0;
	virtual void on_library_item_unregistered(const int layer_id) = 0;
	virtual void on_library_item_changed(const int layer_id, const ItemChange change) = 0;
	virtual void on_library_emitter_changed(const VoxelInstanceEmitter &emitter) = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_INSTANCE_LIBRARY_LISTENER_H
