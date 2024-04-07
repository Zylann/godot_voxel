#include "voxel_tool_buffer.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/math/conv.h"
#include "../util/profiling.h"
#include "funcs.h"

namespace zylann::voxel {

VoxelToolBuffer::VoxelToolBuffer(Ref<godot::VoxelBuffer> vb) {
	ERR_FAIL_COND(vb.is_null());
	_buffer = vb;
	// Editing a buffer is easier if we can partially overlap outside.
	_allow_out_of_bounds = true;
}

bool VoxelToolBuffer::is_area_editable(const Box3i &box) const {
	ERR_FAIL_COND_V(_buffer.is_null(), false);
	return Box3i(Vector3i(), _buffer->get_buffer().get_size()).encloses(box);
}

void VoxelToolBuffer::do_sphere(Vector3 center, float radius) {
	ERR_FAIL_COND(_buffer.is_null());
	ZN_PROFILE_SCOPE();

	VoxelBuffer &vb = _buffer->get_buffer();

	ops::DoShapeSingleBuffer<ops::SdfSphere> op;
	op.shape.center = center;
	op.shape.radius = radius;
	op.shape.sdf_scale = get_sdf_scale();
	op.box = op.shape.get_box().clipped(vb.get_size());
	op.mode = static_cast<ops::Mode>(get_mode());
	op.buffer = &vb;
	op.texture_params = _texture_params;
	op.blocky_value = _value;
	op.channel = get_channel();
	op.strength = get_sdf_strength();

	op();

	_post_edit(op.box);
}

void VoxelToolBuffer::do_box(Vector3i begin, Vector3i end) {
	ERR_FAIL_COND(_buffer.is_null());
	ZN_PROFILE_SCOPE();

	VoxelBuffer &vb = _buffer->get_buffer();

	Vector3iUtil::sort_min_max(begin, end);
	const Box3i box = Box3i::from_min_max(begin, end + Vector3i(1, 1, 1)).clipped(vb.get_size());

	if (_channel == VoxelBuffer::CHANNEL_SDF) {
#if 0
		const float sdf_scale = get_sdf_scale();
		const VoxelBuffer::ChannelId channel = _channel;
		const ops::Mode mode = static_cast<ops::Mode>(_mode);

		// TODO Better quality
		// Not consistent SDF, but should work ok
		box.for_each_cell([&vb, sdf_scale, channel, mode](Vector3i pos) {
			vb.set_voxel_f(ops::sdf_blend(constants::SDF_FAR_INSIDE * sdf_scale, vb.get_voxel_f(pos, channel), mode),
					pos, channel);
		});
#else
		ops::DoShapeSingleBuffer<ops::SdfAxisAlignedBox> op;
		op.shape.center = to_vec3(begin + end) * 0.5;
		op.shape.half_size = to_vec3(end - begin) * 0.5;
		op.shape.sdf_scale = get_sdf_scale();
		op.box = op.shape.get_box().clipped(vb.get_size());
		op.mode = static_cast<ops::Mode>(get_mode());
		op.buffer = &vb;
		op.texture_params = _texture_params;
		op.blocky_value = _value;
		op.channel = get_channel();
		op.strength = get_sdf_strength();

		op();
#endif

	} else {
		const int value = _mode == MODE_REMOVE ? _eraser_value : _value;
		box.for_each_cell([this, value](Vector3i pos) { _set_voxel(pos, value); });
	}

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

void VoxelToolBuffer::paste(Vector3i p_pos, const VoxelBuffer &src, uint8_t channels_mask) {
	ERR_FAIL_COND(_buffer.is_null());

	VoxelBuffer &dst = _buffer->get_buffer();

	Box3i box(p_pos, src.get_size());
	const Vector3i min_noclamp = box.position;
	box.clip(Box3i(Vector3i(), dst.get_size()));

	if (channels_mask == 0) {
		channels_mask = (1 << get_channel());
	}

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	const Vector3i box_max = box.position + box.size;

	for (const uint8_t channel_index : channels) {
		for (int z = box.position.z; z < box_max.z; ++z) {
			const int bz = z - min_noclamp.z;

			for (int x = box.position.x; x < box_max.x; ++x) {
				const int bx = x - min_noclamp.x;

				for (int y = box.position.y; y < box_max.y; ++y) {
					const int by = y - min_noclamp.y;

					const uint64_t v = src.get_voxel(bx, by, bz, channel_index);

					dst.set_voxel(v, x, y, z, channel_index);

					// Overwrite previous metadata
					dst.erase_voxel_metadata(Vector3i(x, y, z));
				}
			}
		}
	}

	dst.copy_voxel_metadata_in_area(src, Box3i(Vector3i(), src.get_size()), p_pos);
}

void VoxelToolBuffer::paste_masked(Vector3i p_pos, Ref<godot::VoxelBuffer> p_voxels, uint8_t channels_mask,
		uint8_t mask_channel, uint64_t mask_value) {
	ERR_FAIL_COND(_buffer.is_null());
	ERR_FAIL_COND(p_voxels.is_null());

	VoxelBuffer &dst = _buffer->get_buffer();
	const VoxelBuffer &src = p_voxels->get_buffer();

	if (channels_mask == 0) {
		channels_mask = (1 << get_channel());
	}

	const SmallVector<uint8_t, VoxelBuffer::MAX_CHANNELS> channels = VoxelBuffer::mask_to_channels_list(channels_mask);

	paste_src_masked(to_span(channels), src, mask_channel, mask_value, dst, p_pos, true);
}

void VoxelToolBuffer::do_path(Span<const Vector3> positions, Span<const float> radii) {
	ZN_PROFILE_SCOPE();
	ZN_ASSERT_RETURN(positions.size() >= 2);
	ZN_ASSERT_RETURN(positions.size() == radii.size());

	ERR_FAIL_COND(_buffer.is_null());
	VoxelBuffer &dst = _buffer->get_buffer();

	// TODO Increase margin a bit with smooth voxels?
	const int margin = 1;

	// Rasterize

	for (unsigned int point_index = 1; point_index < positions.size(); ++point_index) {
		// TODO Could run this in local space so we dont need doubles
		// TODO Apply terrain scale
		const Vector3 p0 = positions[point_index - 1];
		const Vector3 p1 = positions[point_index];

		const float r0 = radii[point_index - 1];
		const float r1 = radii[point_index];

		const Box3i segment_box = ops::get_round_cone_int_bounds(p0, p1, r0, r1).padded(margin);
		if (!Box3i(Vector3i(), dst.get_size()).intersects(segment_box)) {
			continue;
		}

		math::SdfRoundConePrecalc cone;
		cone.a = p0;
		cone.b = p1;
		cone.r1 = r0;
		cone.r2 = r1;
		cone.update();

		ops::SdfRoundCone shape;
		shape.cone = cone;
		shape.sdf_scale = get_sdf_scale();

		const Box3i local_box = segment_box.clipped(Box3i(Vector3i(), dst.get_size()));

		if (get_channel() == VoxelBuffer::CHANNEL_SDF) {
			switch (get_mode()) {
				case MODE_ADD: {
					// TODO Support other depths, format should be accessible from the volume. Or separate encoding?
					ops::SdfOperation16bit<ops::SdfUnion, ops::SdfRoundCone> op;
					op.shape = shape;
					op.op.strength = get_sdf_strength();
					dst.write_box(local_box, VoxelBuffer::CHANNEL_SDF, op, Vector3i());
				} break;

				case MODE_REMOVE: {
					ops::SdfOperation16bit<ops::SdfSubtract, ops::SdfRoundCone> op;
					op.shape = shape;
					op.op.strength = get_sdf_strength();
					dst.write_box(local_box, VoxelBuffer::CHANNEL_SDF, op, Vector3i());
				} break;

				case MODE_SET: {
					ops::SdfOperation16bit<ops::SdfSet, ops::SdfRoundCone> op;
					op.shape = shape;
					op.op.strength = get_sdf_strength();
					dst.write_box(local_box, VoxelBuffer::CHANNEL_SDF, op, Vector3i());
				} break;

				case MODE_TEXTURE_PAINT: {
					ops::TextureBlendOp<ops::SdfRoundCone> op;
					op.shape = shape;
					op.texture_params = _texture_params;
					dst.write_box_2_template<ops::TextureBlendOp<ops::SdfRoundCone>, uint16_t, uint16_t>(
							local_box, VoxelBuffer::CHANNEL_INDICES, VoxelBuffer::CHANNEL_WEIGHTS, op, Vector3i());
				} break;

				default:
					ERR_PRINT("Unknown mode");
					break;
			}

		} else {
			ops::BlockySetOperation<uint32_t, ops::SdfRoundCone> op;
			op.shape = shape;
			op.value = get_value();
			dst.write_box(local_box, get_channel(), op, Vector3i());
		}
	}
}

} // namespace zylann::voxel
