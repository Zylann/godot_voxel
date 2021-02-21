#include "voxel_tool_buffer.h"
#include "../storage/voxel_buffer.h"

VoxelToolBuffer::VoxelToolBuffer(Ref<VoxelBuffer> vb) {
	ERR_FAIL_COND(vb.is_null());
	_buffer = vb;
}

bool VoxelToolBuffer::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_buffer.is_null(), false);
	return Rect3i(Vector3i(), _buffer->get_size()).encloses(box);
}

uint64_t VoxelToolBuffer::_get_voxel(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_voxel(pos, _channel);
}

float VoxelToolBuffer::_get_voxel_f(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_voxel_f(pos.x, pos.y, pos.z, _channel);
}

void VoxelToolBuffer::_set_voxel(Vector3i pos, uint64_t v) {
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

void VoxelToolBuffer::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_buffer.is_null());
	_buffer->set_voxel_metadata(pos, meta);
}

Variant VoxelToolBuffer::get_voxel_metadata(Vector3i pos) {
	ERR_FAIL_COND_V(_buffer.is_null(), Variant());
	return _buffer->get_voxel_metadata(pos);
}

void VoxelToolBuffer::paste(Vector3i p_pos, Ref<VoxelBuffer> p_voxels, uint8_t channels_mask, uint64_t mask_value) {
	ERR_FAIL_COND(_buffer.is_null());
	ERR_FAIL_COND(p_voxels.is_null());

	VoxelBuffer *dst = *_buffer;
	const VoxelBuffer *src = *p_voxels;

	Rect3i box(p_pos, p_voxels->get_size());
	const Vector3i min_noclamp = box.pos;
	box.clip(Rect3i(Vector3i(), _buffer->get_size()));

	if (channels_mask == 0) {
		channels_mask = (1 << get_channel());
	}

	unsigned int channel_count;
	FixedArray<uint8_t, VoxelBuffer::MAX_CHANNELS> channels =
			VoxelBuffer::mask_to_channels_list(channels_mask, channel_count);

	const Vector3i box_max = box.pos + box.size;

	for (unsigned int ci = 0; ci < channel_count; ++ci) {
		const unsigned int channel_index = channels[ci];

		for (int z = box.pos.z; z < box_max.z; ++z) {
			const int bz = z - min_noclamp.z;

			for (int x = box.pos.x; x < box_max.x; ++x) {
				const int bx = x - min_noclamp.x;

				for (int y = box.pos.y; y < box_max.y; ++y) {
					const int by = y - min_noclamp.y;

					const uint64_t v = src->get_voxel(bx, by, bz, channel_index);
					if (v != mask_value) {
						dst->set_voxel(v, x, y, z, channel_index);

						// Overwrite previous metadata
						dst->set_voxel_metadata(Vector3i(x, y, z), Variant());
					}
				}
			}
		}
	}

	_buffer->copy_voxel_metadata_in_area(p_voxels, Rect3i(Vector3i(), p_voxels->get_size()), p_pos);
}
