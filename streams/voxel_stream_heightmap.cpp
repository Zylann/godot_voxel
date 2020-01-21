#include "voxel_stream_heightmap.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

VoxelStreamHeightmap::VoxelStreamHeightmap() {
	_heightmap.settings.range.base = -50.0;
	_heightmap.settings.range.span = 200.0;
	_heightmap.settings.mode = HeightmapSdf::SDF_VERTICAL_AVERAGE;
}

void VoxelStreamHeightmap::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
	if (_channel != VoxelBuffer::CHANNEL_SDF) {
		_heightmap.clear_cache();
	}
}

VoxelBuffer::ChannelId VoxelStreamHeightmap::get_channel() const {
	return _channel;
}

void VoxelStreamHeightmap::set_sdf_mode(SdfMode mode) {
	ERR_FAIL_INDEX(mode, SDF_MODE_COUNT);
	_heightmap.settings.mode = (HeightmapSdf::Mode)mode;
}

VoxelStreamHeightmap::SdfMode VoxelStreamHeightmap::get_sdf_mode() const {
	return (VoxelStreamHeightmap::SdfMode)_heightmap.settings.mode;
}

void VoxelStreamHeightmap::set_height_base(float base) {
	_heightmap.settings.range.base = base;
}

float VoxelStreamHeightmap::get_height_base() const {
	return _heightmap.settings.range.base;
}

void VoxelStreamHeightmap::set_height_range(float range) {
	_heightmap.settings.range.span = range;
}

float VoxelStreamHeightmap::get_height_range() const {
	return _heightmap.settings.range.span;
}

void VoxelStreamHeightmap::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamHeightmap::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamHeightmap::get_channel);

	ClassDB::bind_method(D_METHOD("set_sdf_mode", "mode"), &VoxelStreamHeightmap::set_sdf_mode);
	ClassDB::bind_method(D_METHOD("get_sdf_mode"), &VoxelStreamHeightmap::get_sdf_mode);

	ClassDB::bind_method(D_METHOD("set_height_base", "base"), &VoxelStreamHeightmap::set_height_base);
	ClassDB::bind_method(D_METHOD("get_height_base"), &VoxelStreamHeightmap::get_height_base);

	ClassDB::bind_method(D_METHOD("set_height_range", "range"), &VoxelStreamHeightmap::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelStreamHeightmap::get_height_range);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_mode", PROPERTY_HINT_ENUM, HeightmapSdf::MODE_HINT_STRING), "set_sdf_mode", "get_sdf_mode");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_base"), "set_height_base", "get_height_base");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "height_range"), "set_height_range", "get_height_range");

	BIND_ENUM_CONSTANT(SDF_VERTICAL);
	BIND_ENUM_CONSTANT(SDF_VERTICAL_AVERAGE);
	BIND_ENUM_CONSTANT(SDF_SEGMENT);
}
