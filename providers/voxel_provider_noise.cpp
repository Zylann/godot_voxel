#include "voxel_provider_noise.h"

void VoxelProviderNoise::set_noise(Ref<OpenSimplexNoise> noise) {
	_noise = noise;
}

Ref<OpenSimplexNoise> VoxelProviderNoise::get_noise() const {
	return _noise;
}

void VoxelProviderNoise::set_height_start(real_t y) {
	_height_start = y;
}

real_t VoxelProviderNoise::get_height_start() const {
	return _height_start;
}

void VoxelProviderNoise::set_height_range(real_t hrange) {
	_height_range = hrange;
}

real_t VoxelProviderNoise::get_height_range() const {
	return _height_range;
}

void VoxelProviderNoise::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(out_buffer.is_null());
	ERR_FAIL_COND(_noise.is_null());

	OpenSimplexNoise &noise = **_noise;
	VoxelBuffer &buffer = **out_buffer;

	if (origin_in_voxels.y > _height_start + _height_range) {

		buffer.clear_channel_f(VoxelBuffer::CHANNEL_ISOLEVEL, 100.0);

	} else if (origin_in_voxels.y + (buffer.get_size().y << lod) < _height_start) {

		buffer.clear_channel_f(VoxelBuffer::CHANNEL_ISOLEVEL, -100.0);

	} else {

		float iso_scale = noise.get_period() * 0.1;

		for (int z = 0; z < buffer.get_size().z; ++z) {
			for (int x = 0; x < buffer.get_size().x; ++x) {
				for (int y = 0; y < buffer.get_size().y; ++y) {

					float ly = origin_in_voxels.y + (y << lod);

					float n = noise.get_noise_3d(
							origin_in_voxels.x + (x << lod),
							ly,
							origin_in_voxels.z + (z << lod));

					float t = (ly - _height_start) / _height_range;

					float d = (n + 2.0 * t - 1.0) * iso_scale;

					buffer.set_voxel_f(d, x, y, z, VoxelBuffer::CHANNEL_ISOLEVEL);
					// TODO Support for blocky voxels
				}
			}
		}
	}
}

void VoxelProviderNoise::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelProviderNoise::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelProviderNoise::get_noise);

	ClassDB::bind_method(D_METHOD("set_height_start", "hstart"), &VoxelProviderNoise::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelProviderNoise::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "hrange"), &VoxelProviderNoise::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelProviderNoise::get_height_range);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
}
