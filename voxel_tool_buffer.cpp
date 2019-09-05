#include "voxel_tool_buffer.h"
#include "voxel_buffer.h"

// TODO Implement VoxelBuffer version

VoxelToolBuffer::VoxelToolBuffer(Ref<VoxelBuffer> vb) {
	ERR_FAIL_COND(vb.is_null());
	_buffer = vb;
}

bool VoxelToolBuffer::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_buffer.is_null(), true);
	ERR_PRINT("Not implemented");
	return false;
}

int VoxelToolBuffer::_get_voxel(Vector3i pos) {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	ERR_PRINT("Not implemented");
	return 0;
}

float VoxelToolBuffer::_get_voxel_f(Vector3i pos) {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	ERR_PRINT("Not implemented");
	return 0;
}

void VoxelToolBuffer::_set_voxel(Vector3i pos, int v) {
	ERR_FAIL_COND(_buffer.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelToolBuffer::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_buffer.is_null());
	ERR_PRINT("Not implemented");
}

void VoxelToolBuffer::_post_edit(const Rect3i &box) {
	ERR_FAIL_COND(_buffer.is_null());
	ERR_PRINT("Not implemented");
}
