#include "voxel_tool_buffer.h"
#include "voxel_buffer.h"

VoxelToolBuffer::VoxelToolBuffer(Ref<VoxelBuffer> vb) {
	ERR_FAIL_COND(vb.is_null());
	_buffer = vb;
}

bool VoxelToolBuffer::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_buffer.is_null(), false);
	return Rect3i(Vector3i(), _buffer->get_size()).encloses(box);
}

int VoxelToolBuffer::_get_voxel(Vector3i pos) {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_voxel(pos, _channel);
}

float VoxelToolBuffer::_get_voxel_f(Vector3i pos) {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_voxel_f(pos.x, pos.y, pos.z, _channel);
}

void VoxelToolBuffer::_set_voxel(Vector3i pos, int v) {
	ERR_FAIL_COND(_buffer.is_null());
	return _buffer->set_voxel(v, pos, _channel);
}

void VoxelToolBuffer::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_buffer.is_null());
	return _buffer->set_voxel_f(v, pos.x, pos.y, pos.z, _channel);
}

void VoxelToolBuffer::_post_edit(const Rect3i &box) {
	ERR_FAIL_COND(_buffer.is_null());
	// Nothing special to do
}
