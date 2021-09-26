#include "voxel_generator_noise.h"
#include <core/core_string_names.h>
#include <core/engine.h>

VoxelGeneratorNoise::VoxelGeneratorNoise() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// Have one by default in editor
		Ref<OpenSimplexNoise> noise;
		noise.instance();
		set_noise(noise);
	}
#endif
}

VoxelGeneratorNoise::~VoxelGeneratorNoise() {
}

void VoxelGeneratorNoise::set_noise(Ref<OpenSimplexNoise> noise) {
	if (_noise == noise) {
		return;
	}
	if (_noise.is_valid()) {
		_noise->disconnect(CoreStringNames::get_singleton()->changed, this, "_on_noise_changed");
	}
	_noise = noise;
	Ref<OpenSimplexNoise> copy;
	if (_noise.is_valid()) {
		_noise->connect(CoreStringNames::get_singleton()->changed, this, "_on_noise_changed");
		// The OpenSimplexNoise resource is not thread-safe so we make a copy of it for use in threads
		copy = _noise->duplicate();
	}
	// The OpenSimplexNoise resource is not thread-safe so we make a copy of it for use in threads
	RWLockWrite wlock(_parameters_lock);
	_parameters.noise = copy;
}

void VoxelGeneratorNoise::_on_noise_changed() {
	ERR_FAIL_COND(_noise.is_null());
	RWLockWrite wlock(_parameters_lock);
	_parameters.noise = _noise->duplicate();
}

void VoxelGeneratorNoise::set_channel(VoxelBuffer::ChannelId p_channel) {
	VoxelBufferInternal::ChannelId channel = VoxelBufferInternal::ChannelId(p_channel);
	ERR_FAIL_INDEX(channel, VoxelBufferInternal::MAX_CHANNELS);
	bool changed = false;
	{
		RWLockWrite wlock(_parameters_lock);
		if (_parameters.channel != channel) {
			_parameters.channel = channel;
			changed = true;
		}
	}
	if (changed) {
		emit_changed();
	}
}

VoxelBuffer::ChannelId VoxelGeneratorNoise::get_channel() const {
	RWLockRead rlock(_parameters_lock);
	return VoxelBuffer::ChannelId(_parameters.channel);
}

int VoxelGeneratorNoise::get_used_channels_mask() const {
	RWLockRead rlock(_parameters_lock);
	return (1 << _parameters.channel);
}

Ref<OpenSimplexNoise> VoxelGeneratorNoise::get_noise() const {
	return _noise;
}

void VoxelGeneratorNoise::set_height_start(real_t y) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.height_start = y;
}

real_t VoxelGeneratorNoise::get_height_start() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height_start;
}

void VoxelGeneratorNoise::set_height_range(real_t hrange) {
	if (hrange < 0.1f) {
		hrange = 0.1f;
	}
	RWLockWrite wlock(_parameters_lock);
	_parameters.height_range = hrange;
}

real_t VoxelGeneratorNoise::get_height_range() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.height_range;
}

// For isosurface use cases, noise can be "shaped" by calculating only the first octave,
// and discarding the next ones if beyond some distance away from the isosurface,
// because then we assume next octaves won't change the sign (which crosses the surface).
// This might reduce accuracy in some areas, but it speeds up the results.
static inline float get_shaped_noise(OpenSimplexNoise &noise, float x, float y, float z, float threshold, float bias) {
	x /= noise.get_period();
	y /= noise.get_period();
	z /= noise.get_period();

	float sum = noise._get_octave_noise_3d(0, x, y, z);

	// A default value for `threshold` would be `persistence`
	if (sum + bias > threshold || sum + bias < -threshold) {
		// Assume next octaves will not change sign of noise
		return sum;
	}

	float amp = 1.0;
	float max = 1.0;

	int i = 0;
	while (++i < noise.get_octaves()) {
		x *= noise.get_lacunarity();
		y *= noise.get_lacunarity();
		z *= noise.get_lacunarity();
		amp *= noise.get_persistence();
		max += amp;
		sum += noise._get_octave_noise_3d(i, x, y, z) * amp;
	}

	return sum / max;
}

VoxelGenerator::Result VoxelGeneratorNoise::generate_block(VoxelBlockRequest &input) {
	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	ERR_FAIL_COND_V(params.noise.is_null(), Result());

	OpenSimplexNoise &noise = **params.noise;
	VoxelBufferInternal &buffer = input.voxel_buffer;
	Vector3i origin_in_voxels = input.origin_in_voxels;
	int lod = input.lod;

	int isosurface_lower_bound = static_cast<int>(Math::floor(params.height_start));
	int isosurface_upper_bound = static_cast<int>(Math::ceil(params.height_start + params.height_range));

	const int air_type = 0;
	const int matter_type = 1;
	const int air_color = 0;
	const int matter_color = 1;

	Result result;

	if (origin_in_voxels.y >= isosurface_upper_bound) {
		// Fill with air
		if (params.channel == VoxelBufferInternal::CHANNEL_SDF) {
			buffer.clear_channel_f(params.channel, 100.0);
		} else if (params.channel == VoxelBufferInternal::CHANNEL_TYPE) {
			buffer.clear_channel(params.channel, air_type);
		} else if (params.channel == VoxelBufferInternal::CHANNEL_COLOR) {
			buffer.clear_channel(params.channel, air_color);
		}
		result.max_lod_hint = true;

	} else if (origin_in_voxels.y + (buffer.get_size().y << lod) < isosurface_lower_bound) {
		// Fill with matter
		if (params.channel == VoxelBufferInternal::CHANNEL_SDF) {
			buffer.clear_channel_f(params.channel, -100.0);
		} else if (params.channel == VoxelBufferInternal::CHANNEL_TYPE) {
			buffer.clear_channel(params.channel, matter_type);
		} else if (params.channel == VoxelBufferInternal::CHANNEL_COLOR) {
			buffer.clear_channel(params.channel, matter_color);
		}
		result.max_lod_hint = true;

	} else {
		const float iso_scale = noise.get_period() * 0.1;
		const Vector3i size = buffer.get_size();
		const float height_range_inv = 1.f / params.height_range;
		const float one_minus_persistence = 1.f - noise.get_persistence();

		for (int z = 0; z < size.z; ++z) {
			int lz = origin_in_voxels.z + (z << lod);

			for (int x = 0; x < size.x; ++x) {
				int lx = origin_in_voxels.x + (x << lod);

				for (int y = 0; y < size.y; ++y) {
					const int ly = origin_in_voxels.y + (y << lod);

					if (ly < isosurface_lower_bound) {
						// Below is only matter
						if (params.channel == VoxelBufferInternal::CHANNEL_SDF) {
							buffer.set_voxel_f(-1, x, y, z, params.channel);
						} else if (params.channel == VoxelBufferInternal::CHANNEL_TYPE) {
							buffer.set_voxel(matter_type, x, y, z, params.channel);
						} else if (params.channel == VoxelBufferInternal::CHANNEL_COLOR) {
							buffer.set_voxel(matter_color, x, y, z, params.channel);
						}
						continue;

					} else if (ly >= isosurface_upper_bound) {
						// Above is only air
						if (params.channel == VoxelBufferInternal::CHANNEL_SDF) {
							buffer.set_voxel_f(1, x, y, z, params.channel);
						} else if (params.channel == VoxelBufferInternal::CHANNEL_TYPE) {
							buffer.set_voxel(air_type, x, y, z, params.channel);
						} else if (params.channel == VoxelBufferInternal::CHANNEL_COLOR) {
							buffer.set_voxel(air_color, x, y, z, params.channel);
						}
						continue;
					}

					// Bias is what makes noise become "matter" the lower we go, and "air" the higher we go
					float t = (ly - params.height_start) * height_range_inv;
					float bias = 2.0 * t - 1.0;

					// We are near the isosurface, need to calculate noise value
					float n = get_shaped_noise(noise, lx, ly, lz, one_minus_persistence, bias);
					float d = (n + bias) * iso_scale;

					if (params.channel == VoxelBufferInternal::CHANNEL_SDF) {
						buffer.set_voxel_f(d, x, y, z, params.channel);
					} else if (params.channel == VoxelBufferInternal::CHANNEL_TYPE && d < 0) {
						buffer.set_voxel(matter_type, x, y, z, params.channel);
					} else if (params.channel == VoxelBufferInternal::CHANNEL_COLOR && d < 0) {
						buffer.set_voxel(matter_color, x, y, z, params.channel);
					}
				}
			}
		}
	}

	return result;
}

void VoxelGeneratorNoise::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelGeneratorNoise::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelGeneratorNoise::get_channel);

	ClassDB::bind_method(D_METHOD("set_noise", "noise"), &VoxelGeneratorNoise::set_noise);
	ClassDB::bind_method(D_METHOD("get_noise"), &VoxelGeneratorNoise::get_noise);

	ClassDB::bind_method(D_METHOD("set_height_start", "hstart"), &VoxelGeneratorNoise::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelGeneratorNoise::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "hrange"), &VoxelGeneratorNoise::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelGeneratorNoise::get_height_range);

	ClassDB::bind_method(D_METHOD("_on_noise_changed"), &VoxelGeneratorNoise::_on_noise_changed);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "noise", PROPERTY_HINT_RESOURCE_TYPE, "OpenSimplexNoise"), "set_noise", "get_noise");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
}
