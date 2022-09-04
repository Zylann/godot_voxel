#include "voxel_data_block_enter_info.h"
#include "../storage/voxel_buffer_gd.h"

namespace zylann::voxel {

int VoxelDataBlockEnterInfo::_b_get_network_peer_id() const {
	return network_peer_id;
}

Ref<gd::VoxelBuffer> VoxelDataBlockEnterInfo::_b_get_voxels() const {
	ERR_FAIL_COND_V(!voxel_block.has_voxels(), Ref<gd::VoxelBuffer>());
	std::shared_ptr<VoxelBufferInternal> vbi = voxel_block.get_voxels_shared();
	Ref<gd::VoxelBuffer> vb = gd::VoxelBuffer::create_shared(vbi);
	return vb;
}

Vector3i VoxelDataBlockEnterInfo::_b_get_position() const {
	return block_position;
}

int VoxelDataBlockEnterInfo::_b_get_lod_index() const {
	return voxel_block.get_lod_index();
}

bool VoxelDataBlockEnterInfo::_b_are_voxels_edited() const {
	return voxel_block.is_edited();
}

void VoxelDataBlockEnterInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_network_peer_id"), &VoxelDataBlockEnterInfo::_b_get_network_peer_id);
	ClassDB::bind_method(D_METHOD("get_voxels"), &VoxelDataBlockEnterInfo::_b_get_voxels);
	ClassDB::bind_method(D_METHOD("get_position"), &VoxelDataBlockEnterInfo::_b_get_position);
	ClassDB::bind_method(D_METHOD("get_lod_index"), &VoxelDataBlockEnterInfo::_b_get_lod_index);
	ClassDB::bind_method(D_METHOD("are_voxels_edited"), &VoxelDataBlockEnterInfo::_b_are_voxels_edited);
}

} // namespace zylann::voxel
