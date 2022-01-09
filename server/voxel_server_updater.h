#ifndef VOXEL_SERVER_UPDATER_H
#define VOXEL_SERVER_UPDATER_H

#include <scene/main/node.h>

namespace zylann::voxel {

// TODO Hack to make VoxelServer update... need ways to integrate callbacks from main loop!
class VoxelServerUpdater : public Node {
	GDCLASS(VoxelServerUpdater, Node)
public:
	~VoxelServerUpdater();
	static void ensure_existence(SceneTree *st);

protected:
	void _notification(int p_what);

private:
	VoxelServerUpdater();
};

} // namespace zylann::voxel

#endif // VOXEL_SERVER_UPDATER_H
