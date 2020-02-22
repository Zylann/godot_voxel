#include "voxel_generator_heightmap.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

VoxelGeneratorHeightmap::VoxelGeneratorHeightmap() {
}

void VoxelGeneratorHeightmap::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	if (_channel != channel) {
		_channel = channel;
		emit_changed();
	}
}

VoxelBuffer::ChannelId VoxelGeneratorHeightmap::get_channel() const {
	return _channel;
}

int VoxelGeneratorHeightmap::get_used_channels_mask() const {
	return (1 << _channel);
}

void VoxelGeneratorHeightmap::set_height_start(float start) {
	_range.start = start;
}

float VoxelGeneratorHeightmap::get_height_start() const {
	return _range.start;
}

void VoxelGeneratorHeightmap::set_height_range(float range) {
	_range.height = range;
}

float VoxelGeneratorHeightmap::get_height_range() const {
	return _range.height;
}

void VoxelGeneratorHeightmap::set_iso_scale(float iso_scale) {
	_iso_scale = iso_scale;
}

float VoxelGeneratorHeightmap::get_iso_scale() const {
	return _iso_scale;
}

void VoxelGeneratorHeightmap::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelGeneratorHeightmap::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelGeneratorHeightmap::get_channel);

	ClassDB::bind_method(D_METHOD("set_height_start", "start"), &VoxelGeneratorHeightmap::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelGeneratorHeightmap::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "range"), &VoxelGeneratorHeightmap::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelGeneratorHeightmap::get_height_range);

	ClassDB::bind_method(D_METHOD("set_iso_scale", "scale"), &VoxelGeneratorHeightmap::set_iso_scale);
	ClassDB::bind_method(D_METHOD("get_iso_scale"), &VoxelGeneratorHeightmap::get_iso_scale);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "iso_scale"), "set_iso_scale", "get_iso_scale");
}
