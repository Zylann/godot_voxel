#include "voxel_stream_noise.h"

void VoxelStreamNoise::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
}

VoxelBuffer::ChannelId VoxelStreamNoise::get_channel() const {
	return _channel;
}

void VoxelStreamNoise::set_noise(Ref<OpenSimplexNoise> noise) {
	_noise = noise;
}

Ref<OpenSimplexNoise> VoxelStreamNoise::get_noise() const {
	return _noise;
}

void VoxelStreamNoise::set_height_start(real_t y) {
	_height_start = y;
}

real_t VoxelStreamNoise::get_height_start() const {
	return _height_start;
}

void VoxelStreamNoise::set_height_range(real_t hrange) {
	_height_range = hrange;
}

real_t VoxelStreamNoise::get_height_range() const {
	return _height_range;
}

void VoxelStreamNoise::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(out_buffer.is_null());
	ERR_FAIL_COND(_noise.is_null());

	OpenSimplexNoise &noise = **_noise;
	VoxelBuffer &buffer = **out_buffer;

	if (origin_in_voxels.y > _height_start + _height_range) {

		if (_channel == VoxelBuffer::CHANNEL_SDF) {
			buffer.clear_channel_f(_channel, 100.0);
		}
		else if (_channel == VoxelBuffer::CHANNEL_TYPE) {
			buffer.clear_channel(_channel, 0);
		}

	} else if (origin_in_voxels.y + (buffer.get_size().y << lod) < _height_start) {

		if (_channel == VoxelBuffer::CHANNEL_SDF) {
			buffer.clear_channel_f(_channel, -100.0);
		}
		else if (_channel == VoxelBuffer::CHANNEL_TYPE) {
			buffer.clear_channel(_channel, 1);
		}

	} else {

		// TODO Proper noise optimization
		// Prefetching was much faster, but it introduced LOD inconsistencies into the data itself, causing cracks.
		// Need to implement it properly, or use a different noise library.

		/*FloatBuffer3D &noise_buffer = _noise_buffer;
		const int noise_buffer_step = 2;

		Vector3i noise_buffer_size = buffer.get_size() / noise_buffer_step + Vector3i(1);
		if (noise_buffer.get_size() != noise_buffer_size) {
			noise_buffer.create(noise_buffer_size);
		}

		// Cache noise at lower grid resolution and interpolate after, much cheaper
		for (int z = 0; z < noise_buffer.get_size().z; ++z) {
			for (int x = 0; x < noise_buffer.get_size().x; ++x) {
				for (int y = 0; y < noise_buffer.get_size().y; ++y) {

					float lx = origin_in_voxels.x + (x << lod) * noise_buffer_step;
					float ly = origin_in_voxels.y + (y << lod) * noise_buffer_step;
					float lz = origin_in_voxels.z + (z << lod) * noise_buffer_step;

					float n = noise.get_noise_3d(lx, ly, lz);

					noise_buffer.set(x, y, z, n);
				}
			}
		}*/

		float iso_scale = noise.get_period() * 0.1;
		//float noise_buffer_scale = 1.f / static_cast<float>(noise_buffer_step);

		Vector3i size = buffer.get_size();

		for (int z = 0; z < size.z; ++z) {
			for (int x = 0; x < size.x; ++x) {
				for (int y = 0; y < size.y; ++y) {

					float lx = origin_in_voxels.x + (x << lod);
					float ly = origin_in_voxels.y + (y << lod);
					float lz = origin_in_voxels.z + (z << lod);

					//float n = noise_buffer.get_trilinear(x * noise_buffer_scale, y * noise_buffer_scale, z * noise_buffer_scale);
					float n = noise.get_noise_3d(lx, ly, lz);
					float t = (ly - _height_start) / _height_range;
					float d = (n + 2.0 * t - 1.0) * iso_scale;

					if (_channel == VoxelBuffer::CHANNEL_SDF) {
						buffer.set_voxel_f(d, x, y, z, _channel);
					}
					else if (_channel == VoxelBuffer::CHANNEL_TYPE && d < 0) {
						buffer.set_voxel(1, x, y, z, _channel);
					}
				}
			}
		}
	}
}

void VoxelStreamNoise::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelStreamNoise::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelStreamNoise::get_noise);

	ClassDB::bind_method(D_METHOD("set_height_start", "hstart"), &VoxelStreamNoise::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelStreamNoise::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "hrange"), &VoxelStreamNoise::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelStreamNoise::get_height_range);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamNoise::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamNoise::get_channel);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
}
