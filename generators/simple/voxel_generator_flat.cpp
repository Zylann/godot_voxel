#include "voxel_generator_flat.h"

namespace zylann::voxel {

VoxelGeneratorFlat::VoxelGeneratorFlat() {}

VoxelGeneratorFlat::~VoxelGeneratorFlat() {}

void VoxelGeneratorFlat::set_channel(VoxelBuffer::ChannelId p_channel) {
	ERR_FAIL_INDEX(p_channel, VoxelBuffer::MAX_CHANNELS);
	bool changed = false;
	{
		RWLockWrite wlock(_parameters_lock);
		if (_parameters.channel != p_channel) {
			_parameters.channel = p_channel;
			changed = true;
		}
	}
	if (changed) {
		emit_changed();
	}
}

VoxelBuffer::ChannelId VoxelGeneratorFlat::get_channel() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.channel;
}

int VoxelGeneratorFlat::get_used_channels_mask() const {
	RWLockRead rlock(_parameters_lock);
	return (1 << _parameters.channel);
}

void VoxelGeneratorFlat::set_voxel_type(int t) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.voxel_type = t;
}

int VoxelGeneratorFlat::get_voxel_type() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.voxel_type;
}

void VoxelGeneratorFlat::set_height(float h) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.height = h;
}

float VoxelGeneratorFlat::get_height() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height;
}

VoxelGenerator::Result VoxelGeneratorFlat::generate_block(VoxelGenerator::VoxelQueryData &input) {
	Result result;

	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	VoxelBuffer &out_buffer = input.voxel_buffer;
	const Vector3i origin = input.origin_in_voxels;
	const int channel = params.channel;
	const Vector3i bs = out_buffer.get_size();
	const bool use_sdf = channel == VoxelBuffer::CHANNEL_SDF;
	const float margin = 1 << input.lod;
	const int lod = input.lod;

	if (origin.y > params.height + margin) {
		// The bottom of the block is above the highest ground can go (default is air)
		result.max_lod_hint = true;
		return result;
	}
	if (origin.y + (bs.y << lod) < params.height - margin) {
		// The top of the block is below the lowest ground can go
		if (use_sdf) {
			// Not consistent SDF but should work ok
			out_buffer.clear_channel_f(params.channel, -100.0);
		} else {
			out_buffer.clear_channel(params.channel, params.voxel_type);
		}
		result.max_lod_hint = true;
		return result;
	}

	const int stride = 1 << lod;

	if (use_sdf) {
		int gz = origin.z;
		for (int z = 0; z < bs.z; ++z, gz += stride) {
			int gx = origin.x;
			for (int x = 0; x < bs.x; ++x, gx += stride) {
				int gy = origin.y;
				for (int y = 0; y < bs.y; ++y, gy += stride) {
					const float sdf = params.iso_scale * (gy - params.height);
					out_buffer.set_voxel_f(sdf, x, y, z, channel);
				}

			} // for x
		} // for z

	} else {
		// Blocky

		const float rh_world = params.height - origin.y;
		const int irh_world = static_cast<int>(rh_world);
		if (irh_world > 0) {
			const int irh_voxels = math::min(math::arithmetic_rshift(irh_world, input.lod), bs.y);
			out_buffer.fill_area(params.voxel_type, Vector3i(0, 0, 0), Vector3i(bs.x, irh_voxels, bs.z), channel);
		}
	} // use_sdf

	return result;
}

void VoxelGeneratorFlat::_b_set_channel(godot::VoxelBuffer::ChannelId p_channel) {
	set_channel(VoxelBuffer::ChannelId(p_channel));
}

godot::VoxelBuffer::ChannelId VoxelGeneratorFlat::_b_get_channel() const {
	return godot::VoxelBuffer::ChannelId(get_channel());
}

void VoxelGeneratorFlat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelGeneratorFlat::_b_set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelGeneratorFlat::_b_get_channel);

	ClassDB::bind_method(D_METHOD("set_voxel_type", "id"), &VoxelGeneratorFlat::set_voxel_type);
	ClassDB::bind_method(D_METHOD("get_voxel_type"), &VoxelGeneratorFlat::get_voxel_type);

	ClassDB::bind_method(D_METHOD("set_height", "h"), &VoxelGeneratorFlat::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &VoxelGeneratorFlat::get_height);

	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, godot::VoxelBuffer::CHANNEL_ID_HINT_STRING),
			"set_channel",
			"get_channel"
	);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height"), "set_height", "get_height");
	ADD_PROPERTY(
			PropertyInfo(Variant::INT, "voxel_type", PROPERTY_HINT_RANGE, "0,65536,1"),
			"set_voxel_type",
			"get_voxel_type"
	);
}

} // namespace zylann::voxel
