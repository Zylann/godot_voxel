#ifndef VOXEL_SERVER_UPDATER_H
#define VOXEL_SERVER_UPDATER_H

#include <scene/main/node.h>

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
};

} // namespace zylann::voxel

#endif // VOXEL_SERVER_UPDATER_H
