#include "voxel_tool_buffer.h"
#include "../storage/voxel_buffer.h"
#include "../util/profiling.h"
#include "funcs.h"

VoxelToolBuffer::VoxelToolBuffer(Ref<VoxelBuffer> vb) {
	ERR_FAIL_COND(vb.is_null());
	_buffer = vb;
}

bool VoxelToolBuffer::is_area_editable(const Box3i &box) const {
	ERR_FAIL_COND_V(_buffer.is_null(), false);
	return Box3i(Vector3i(), _buffer->get_buffer().get_size()).encloses(box);
}

void VoxelToolBuffer::do_sphere(Vector3 center, float radius) {
	ERR_FAIL_COND(_buffer.is_null());

	if (_mode != MODE_TEXTURE_PAINT) {
		// TODO Eventually all specialized voxel tools should use lambda writing functions
		VoxelTool::do_sphere(center, radius);
		return;
	}

	VOXEL_PROFILE_SCOPE();

	Box3i box(Vector3i::from_floored(center) - Vector3i(Math::floor(radius)), Vector3i(Math::ceil(radius) * 2));
	box.clip(Box3i(Vector3i(), _buffer->get_buffer().get_size()));

	_buffer->get_buffer().write_box_2_template<TextureBlendSphereOp, uint16_t, uint16_t>(box,
			VoxelBufferInternal::CHANNEL_INDICES,
			VoxelBufferInternal::CHANNEL_WEIGHTS,
			TextureBlendSphereOp(center, radius, _texture_params),
			Vector3i());

	_post_edit(box);
}

uint64_t VoxelToolBuffer::_get_voxel(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_buffer().get_voxel(pos, _channel);
}

float VoxelToolBuffer::_get_voxel_f(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), 0);
	return _buffer->get_buffer().get_voxel_f(pos.x, pos.y, pos.z, _channel);
}

void VoxelToolBuffer::_set_voxel(Vector3i pos, uint64_t v) {
	ERR_FAIL_COND(_buffer.is_null());
	return _buffer->get_buffer().set_voxel(v, pos, _channel);
}

void VoxelToolBuffer::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_buffer.is_null());
	return _buffer->set_voxel_f(v, pos.x, pos.y, pos.z, _channel);
}

void VoxelToolBuffer::_post_edit(const Box3i &box) {
	ERR_FAIL_COND(_buffer.is_null());
	// Nothing special to do
}

void VoxelToolBuffer::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(_buffer.is_null());
	_buffer->get_buffer().set_voxel_metadata(pos, meta);
}

Variant VoxelToolBuffer::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), Variant());
	return _buffer->get_buffer().get_voxel_metadata(pos);
}

void VoxelToolBuffer::paste(Vector3i p_pos, Ref<VoxelBuffer> p_voxels, uint8_t channels_mask, bool use_mask,
		uint64_t mask_value) {
	// TODO Support `use_mask` properly
	if (use_mask) {
		mask_value = 0xffffffffffffffff;
	}

	ERR_FAIL_COND(_buffer.is_null());
	ERR_FAIL_COND(p_voxels.is_null());

	VoxelBufferInternal &dst = _buffer->get_buffer();
	const VoxelBufferInternal &src = p_voxels->get_buffer();

	Box3i box(p_pos, p_voxels->get_buffer().get_size());
	const Vector3i min_noclamp = box.pos;
	box.clip(Box3i(Vector3i(), _buffer->get_buffer().get_size()));

	if (channels_mask == 0) {
		channels_mask = (1 << get_channel());
	}

	unsigned int channel_count;
	FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels =
			VoxelBufferInternal::mask_to_channels_list(channels_mask, channel_count);

	const Vector3i box_max = box.pos + box.size;

	for (unsigned int ci = 0; ci < channel_count; ++ci) {
		const unsigned int channel_index = channels[ci];

		for (int z = box.pos.z; z < box_max.z; ++z) {
			const int bz = z - min_noclamp.z;

			for (int x = box.pos.x; x < box_max.x; ++x) {
				const int bx = x - min_noclamp.x;

				for (int y = box.pos.y; y < box_max.y; ++y) {
					const int by = y - min_noclamp.y;

					const uint64_t v = src.get_voxel(bx, by, bz, channel_index);
					if (v != mask_value) {
						dst.set_voxel(v, x, y, z, channel_index);

						// Overwrite previous metadata
						dst.set_voxel_metadata(Vector3i(x, y, z), Variant());
					}
				}
			}
		}
	}

	_buffer->get_buffer().copy_voxel_metadata_in_area(
			p_voxels->get_buffer(), Box3i(Vector3i(), p_voxels->get_buffer().get_size()), p_pos);
}
