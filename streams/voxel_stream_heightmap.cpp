#include "voxel_stream_heightmap.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

VoxelStreamHeightmap::VoxelStreamHeightmap() {
}

void VoxelStreamHeightmap::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
}

VoxelBuffer::ChannelId VoxelStreamHeightmap::get_channel() const {
	return _channel;
}

void VoxelStreamHeightmap::set_height_start(float start) {
	_range.start = start;
}

float VoxelStreamHeightmap::get_height_start() const {
	return _range.start;
}

void VoxelStreamHeightmap::set_height_range(float range) {
	_range.height = range;
}

float VoxelStreamHeightmap::get_height_range() const {
	return _range.height;
}

void VoxelStreamHeightmap::set_iso_scale(float iso_scale) {
	_iso_scale = iso_scale;
}

float VoxelStreamHeightmap::get_iso_scale() const {
	return _iso_scale;
}

void VoxelStreamHeightmap::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamHeightmap::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamHeightmap::get_channel);

	ClassDB::bind_method(D_METHOD("set_height_start", "start"), &VoxelStreamHeightmap::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelStreamHeightmap::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "range"), &VoxelStreamHeightmap::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelStreamHeightmap::get_height_range);

	ClassDB::bind_method(D_METHOD("set_iso_scale", "scale"), &VoxelStreamHeightmap::set_iso_scale);
	ClassDB::bind_method(D_METHOD("get_iso_scale"), &VoxelStreamHeightmap::get_iso_scale);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "iso_scale"), "set_iso_scale", "get_iso_scale");
}
