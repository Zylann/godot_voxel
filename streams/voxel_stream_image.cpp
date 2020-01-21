#include "voxel_stream_image.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

namespace {

inline float get_height_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

} // namespace

VoxelStreamImage::VoxelStreamImage() {
	_heightmap.settings.range.base = -50.0;
	_heightmap.settings.range.span = 200.0;
	_heightmap.settings.mode = HeightmapSdf::SDF_VERTICAL_AVERAGE;
}

void VoxelStreamImage::set_image(Ref<Image> im) {
	_image = im;
	_cache_dirty = true;
}

Ref<Image> VoxelStreamImage::get_image() const {
	return _image;
}

void VoxelStreamImage::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
	if (_channel != VoxelBuffer::CHANNEL_SDF) {
		_heightmap.clear_cache();
	}
}

VoxelBuffer::ChannelId VoxelStreamImage::get_channel() const {
	return _channel;
}

void VoxelStreamImage::set_sdf_mode(SdfMode mode) {
	ERR_FAIL_INDEX(mode, SDF_MODE_COUNT);
	_heightmap.settings.mode = (HeightmapSdf::Mode)mode;
	_cache_dirty = true;
}

VoxelStreamImage::SdfMode VoxelStreamImage::get_sdf_mode() const {
	return (VoxelStreamImage::SdfMode)_heightmap.settings.mode;
}

void VoxelStreamImage::set_height_base(float base) {
	_heightmap.settings.range.base = base;
	_cache_dirty = true;
}

float VoxelStreamImage::get_height_base() const {
	return _heightmap.settings.range.base;
}

void VoxelStreamImage::set_height_range(float range) {
	_heightmap.settings.range.span = range;
	_cache_dirty = true;
}

float VoxelStreamImage::get_height_range() const {
	return _heightmap.settings.range.span;
}

void VoxelStreamImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(_image.is_null());

	const int dirt = 1;
	VoxelBuffer &out_buffer = **p_out_buffer;
	const Vector3i bs = out_buffer.get_size();
	const int ox = origin_in_voxels.x;
	const int oy = origin_in_voxels.y;
	const int oz = origin_in_voxels.z;

	const int channel = _channel;
	bool use_sdf = channel == VoxelBuffer::CHANNEL_SDF;

	if (oy > get_height_base() + get_height_range()) {
		// The bottom of the block is above the highest ground can go (default is air)
		return;
	}
	if (oy + (bs.y << lod) < get_height_base()) {
		// The top of the block is below the lowest ground can go
		out_buffer.clear_channel(_channel, use_sdf ? 0 : dirt);
		return;
	}

	const int stride = 1 << lod;

	Image &image = **_image;
	image.lock();

	if (use_sdf) {

		if (lod == 0) {
			// When sampling SDF, we may need to precompute values to speed it up
			// Only LOD0 can use a cache though... lower lods would require a much larger cache,
			// otherwise it would interpolate along higher stride, thus voxel values depend on LOD, and then cause discontinuities.

			_heightmap.build_cache(
					[&image](int x, int z) { return get_height_repeat(image, x, z); },
					bs.x, bs.z, ox, oz, stride);
		}

		for (int z = 0; z < bs.z; ++z) {
			for (int x = 0; x < bs.x; ++x) {

				// SDF may vary along the column so we use a helper for more precision

				if (lod == 0) {
					_heightmap.get_column_from_cache(
							[&out_buffer, x, z, channel](int ly, float v) { out_buffer.set_voxel_f(v, x, ly, z, channel); },
							x, oy, z, bs.y, stride);
				} else {
					HeightmapSdf::get_column_stateless(
							[&out_buffer, x, z, channel](int ly, float v) { out_buffer.set_voxel_f(v, x, ly, z, channel); },
							[&image, this](int lx, int lz) { return _heightmap.settings.range.xform(get_height_repeat(image, lx, lz)); },
							_heightmap.settings.mode,
							ox + (x << lod), oy, oz + (z << lod), stride, bs.y);
				}

			} // for x
		} // for z

	} else {
		// Blocky

		int gz = oz;
		for (int z = 0; z < bs.z; ++z, gz += stride) {

			int gx = ox;
			for (int x = 0; x < bs.x; ++x, gx += stride) {

				// Output is blocky, so we can go for just one sample
				float h = _heightmap.settings.range.xform(get_height_repeat(image, gx, gz));
				h -= oy;
				int ih = int(h);
				if (ih > 0) {
					if (ih > bs.y) {
						ih = bs.y;
					}
					out_buffer.fill_area(dirt, Vector3i(x, 0, z), Vector3i(x + 1, ih, z + 1), channel);
				}

			} // for x
		} // for z
	}

	image.lock();

	out_buffer.compress_uniform_channels();
}

void VoxelStreamImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelStreamImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelStreamImage::get_image);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamImage::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamImage::get_channel);

	ClassDB::bind_method(D_METHOD("set_sdf_mode", "mode"), &VoxelStreamImage::set_sdf_mode);
	ClassDB::bind_method(D_METHOD("get_sdf_mode"), &VoxelStreamImage::get_sdf_mode);

	ClassDB::bind_method(D_METHOD("set_height_base", "base"), &VoxelStreamImage::set_height_base);
	ClassDB::bind_method(D_METHOD("get_height_base"), &VoxelStreamImage::get_height_base);

	ClassDB::bind_method(D_METHOD("set_height_range", "range"), &VoxelStreamImage::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelStreamImage::get_height_range);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_mode", PROPERTY_HINT_ENUM, HeightmapSdf::MODE_HINT_STRING), "set_sdf_mode", "get_sdf_mode");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_base"), "set_height_base", "get_height_base");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");

	BIND_ENUM_CONSTANT(SDF_VERTICAL);
	BIND_ENUM_CONSTANT(SDF_VERTICAL_AVERAGE);
	BIND_ENUM_CONSTANT(SDF_SEGMENT);
}
