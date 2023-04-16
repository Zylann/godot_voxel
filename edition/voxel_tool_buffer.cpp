#include "voxel_tool_buffer.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "funcs.h"

namespace zylann::voxel {

VoxelToolBuffer::VoxelToolBuffer(Ref<gd::VoxelBuffer> vb) {
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

	ZN_PROFILE_SCOPE();

	Box3i box(math::floor_to_int(center) - Vector3iUtil::create(Math::floor(radius)),
			Vector3iUtil::create(Math::ceil(radius) * 2));
	box.clip(Box3i(Vector3i(), _buffer->get_buffer().get_size()));

	_buffer->get_buffer().write_box_2_template<ops::TextureBlendSphereOp, uint16_t, uint16_t>(box,
			VoxelBufferInternal::CHANNEL_INDICES, VoxelBufferInternal::CHANNEL_WEIGHTS,
			ops::TextureBlendSphereOp(center, radius, _texture_params), Vector3i());

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
	_buffer->set_voxel_metadata(pos, meta);
}

Variant VoxelToolBuffer::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(_buffer.is_null(), Variant());
	return _buffer->get_voxel_metadata(pos);
}

void VoxelToolBuffer::paste(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask) {
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

					dst.set_voxel(v, x, y, z, channel_index);

					// Overwrite previous metadata
					dst.erase_voxel_metadata(Vector3i(x, y, z));
				}
			}
		}
	}

	_buffer->get_buffer().copy_voxel_metadata_in_area(
			p_voxels->get_buffer(), Box3i(Vector3i(), p_voxels->get_buffer().get_size()), p_pos);
}

void VoxelToolBuffer::paste_masked(Vector3i p_pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask,
		uint8_t mask_channel, uint64_t mask_value) {
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

					const uint64_t mv = src.get_voxel(bx, by, bz, mask_channel);
					if (mv != mask_value) {
						const uint64_t v =
								mask_channel == channel_index ? mv : src.get_voxel(bx, by, bz, channel_index);
						dst.set_voxel(v, x, y, z, channel_index);

						// Overwrite previous metadata
						dst.erase_voxel_metadata(Vector3i(x, y, z));
					}
				}
			}
		}
	}

	_buffer->get_buffer().for_each_voxel_metadata_in_area(
			box, [min_noclamp, &src, &dst, mask_channel, mask_value](Vector3i dst_pos, const VoxelMetadata &meta) {
				const Vector3i src_pos = dst_pos - min_noclamp;
				ZN_ASSERT(src.is_position_valid(src_pos));
				if (src.get_voxel(src_pos, mask_channel) != mask_value) {
					VoxelMetadata *dst_meta = dst.get_or_create_voxel_metadata(dst_pos);
					ZN_ASSERT_RETURN(dst_meta != nullptr);
					dst_meta->copy_from(meta);
				}
			});
}

} // namespace zylann::voxel
