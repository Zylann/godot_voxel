#include "voxel_generator_flat.h"

VoxelGeneratorFlat::VoxelGeneratorFlat() {
}

void VoxelGeneratorFlat::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	if (_channel != channel) {
		_channel = channel;
		emit_changed();
	}
}

VoxelBuffer::ChannelId VoxelGeneratorFlat::get_channel() const {
	return _channel;
}

int VoxelGeneratorFlat::get_used_channels_mask() const {
	return (1 << _channel);
}

void VoxelGeneratorFlat::set_voxel_type(int t) {
	_voxel_type = t;
}

int VoxelGeneratorFlat::get_voxel_type() const {
	return _voxel_type;
}

void VoxelGeneratorFlat::set_height(float h) {
	_height = h;
}

void VoxelGeneratorFlat::generate_block(VoxelBlockRequest &input) {
	ERR_FAIL_COND(input.voxel_buffer.is_null());

	VoxelBuffer &out_buffer = **input.voxel_buffer;
	const Vector3i origin = input.origin_in_voxels;
	const int channel = _channel;
	const Vector3i bs = out_buffer.get_size();
	const bool use_sdf = channel == VoxelBuffer::CHANNEL_SDF;
	const float margin = 1 << input.lod;
	const int lod = input.lod;

	if (origin.y > _height + margin) {
		// The bottom of the block is above the highest ground can go (default is air)
		return;
	}
	if (origin.y + (bs.y << lod) < _height - margin) {
		// The top of the block is below the lowest ground can go
		out_buffer.clear_channel(_channel, use_sdf ? 0 : _voxel_type);
		return;
	}

	const int stride = 1 << lod;

	if (use_sdf) {

		int gz = origin.z;
		for (int z = 0; z < bs.z; ++z, gz += stride) {

			int gx = origin.x;
			for (int x = 0; x < bs.x; ++x, gx += stride) {

				int gy = origin.y;
				for (int y = 0; y < bs.y; ++y, gy += stride) {
					float sdf = _iso_scale * (gy - _height);
					out_buffer.set_voxel_f(sdf, x, y, z, channel);
				}

			} // for x
		} // for z

	} else {
		// Blocky

		int gz = origin.z;
		for (int z = 0; z < bs.z; ++z, gz += stride) {

			int gx = origin.x;
			for (int x = 0; x < bs.x; ++x, gx += stride) {

				float h = _height - origin.y;
				int ih = int(h);
				if (ih > 0) {
					if (ih > bs.y) {
						ih = bs.y;
					}
					out_buffer.fill_area(_voxel_type, Vector3i(x, 0, z), Vector3i(x + 1, ih, z + 1), channel);
				}

			} // for x
		} // for z
	} // use_sdf
}

void VoxelGeneratorFlat::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelGeneratorFlat::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelGeneratorFlat::get_channel);

	ClassDB::bind_method(D_METHOD("set_voxel_type", "id"), &VoxelGeneratorFlat::set_voxel_type);
	ClassDB::bind_method(D_METHOD("get_voxel_type"), &VoxelGeneratorFlat::get_voxel_type);

	ClassDB::bind_method(D_METHOD("set_height", "h"), &VoxelGeneratorFlat::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &VoxelGeneratorFlat::get_height);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "voxel_type", PROPERTY_HINT_RANGE, "0,65536,1"), "set_voxel_type", "get_voxel_type");
}
