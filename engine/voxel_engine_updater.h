#ifndef VOXEL_SERVER_UPDATER_H
#define VOXEL_SERVER_UPDATER_H

#include "../util/godot/classes/node.h"

namespace zylann::voxel {

// TODO Hack to make VoxelEngine update... need ways to integrate callbacks from main loop!
class VoxelEngineUpdater : public Node {
	GDCLASS(VoxelEngineUpdater, Node)
public:
	~VoxelEngineUpdater();
	static void ensure_existence(SceneTree *st);

protected:
	void _notification(int p_what);

private:
	VoxelEngineUpdater();

	// When compiling with GodotCpp, `_bind_methods` is not optional.
	static void _bind_methods() {}
};

} // namespace zylann::voxel

#endif // VOXEL_SERVER_UPDATER_H
