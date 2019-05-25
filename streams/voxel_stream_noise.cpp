#include "voxel_stream_noise.h"

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

		buffer.clear_channel_f(VoxelBuffer::CHANNEL_ISOLEVEL, 100.0);

	} else if (origin_in_voxels.y + (buffer.get_size().y << lod) < _height_start) {

		buffer.clear_channel_f(VoxelBuffer::CHANNEL_ISOLEVEL, -100.0);

	} else {

		FloatBuffer3D &noise_buffer = _noise_buffer;
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
		}

		float iso_scale = noise.get_period() * 0.1;
		float noise_buffer_scale = 1.f / static_cast<float>(noise_buffer_step);

		for (int z = 0; z < buffer.get_size().z; ++z) {
			for (int x = 0; x < buffer.get_size().x; ++x) {
				for (int y = 0; y < buffer.get_size().y; ++y) {

					float ly = origin_in_voxels.y + (y << lod);

					float n = noise_buffer.get_trilinear(x * noise_buffer_scale, y * noise_buffer_scale, z * noise_buffer_scale);
					float t = (ly - _height_start) / _height_range;
					float d = (n + 2.0 * t - 1.0) * iso_scale;

					buffer.set_voxel_f(d, x, y, z, VoxelBuffer::CHANNEL_ISOLEVEL);
					// TODO Support for blocky voxels
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

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
}
