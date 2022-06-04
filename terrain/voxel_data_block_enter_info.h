#ifndef VOXEL_DATA_BLOCK_ENTER_INFO_H
#define VOXEL_DATA_BLOCK_ENTER_INFO_H

#include <core/object/ref_counted.h>

namespace zylann::voxel {

class VoxelDataBlock;

namespace gd {
class VoxelBuffer;
}

// Information sent with data block entering notifications.
// It is a class for script API convenience.
// You may neither create this object on your own, nor keep a reference to it.
class VoxelDataBlockEnterInfo : public Object {
	GDCLASS(VoxelDataBlockEnterInfo, Object)
public:
	int network_peer_id = -1;
	Vector3i block_position;
	VoxelDataBlock *voxel_block = nullptr;

private:
	int _b_get_network_peer_id() const;
	Ref<gd::VoxelBuffer> _b_get_voxels() const;
	Vector3i _b_get_position() const;
	int _b_get_lod_index() const;
	bool _b_are_voxels_edited() const;
	//int _b_viewer_id() const;

	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_DATA_BLOCK_ENTER_INFO_H
