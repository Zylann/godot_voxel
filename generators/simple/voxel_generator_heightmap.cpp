#include "voxel_generator_heightmap.h"
#include "../../util/fixed_array.h"
#include "../../util/span.h"

namespace zylann::voxel {

VoxelGeneratorHeightmap::VoxelGeneratorHeightmap() {}

VoxelGeneratorHeightmap::~VoxelGeneratorHeightmap() {}

void VoxelGeneratorHeightmap::set_channel(VoxelBufferInternal::ChannelId p_channel) {
	ERR_FAIL_INDEX(p_channel, VoxelBufferInternal::MAX_CHANNELS);
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

VoxelBufferInternal::ChannelId VoxelGeneratorHeightmap::get_channel() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.channel;
}

int VoxelGeneratorHeightmap::get_used_channels_mask() const {
	RWLockRead rlock(_parameters_lock);
	return (1 << _parameters.channel);
}

void VoxelGeneratorHeightmap::set_height_start(float start) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.range.start = start;
}

float VoxelGeneratorHeightmap::get_height_start() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.range.start;
}

void VoxelGeneratorHeightmap::set_height_range(float range) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.range.height = range;
}

float VoxelGeneratorHeightmap::get_height_range() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.range.height;
}

void VoxelGeneratorHeightmap::set_iso_scale(float iso_scale) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.iso_scale = iso_scale;
}

float VoxelGeneratorHeightmap::get_iso_scale() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.iso_scale;
}

void VoxelGeneratorHeightmap::_b_set_channel(gd::VoxelBuffer::ChannelId p_channel) {
	set_channel(VoxelBufferInternal::ChannelId(p_channel));
}

gd::VoxelBuffer::ChannelId VoxelGeneratorHeightmap::_b_get_channel() const {
	return gd::VoxelBuffer::ChannelId(get_channel());
}

void VoxelGeneratorHeightmap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelGeneratorHeightmap::_b_set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelGeneratorHeightmap::_b_get_channel);

	ClassDB::bind_method(D_METHOD("set_height_start", "start"), &VoxelGeneratorHeightmap::set_height_start);
	ClassDB::bind_method(D_METHOD("get_height_start"), &VoxelGeneratorHeightmap::get_height_start);

	ClassDB::bind_method(D_METHOD("set_height_range", "range"), &VoxelGeneratorHeightmap::set_height_range);
	ClassDB::bind_method(D_METHOD("get_height_range"), &VoxelGeneratorHeightmap::get_height_range);

	ClassDB::bind_method(D_METHOD("set_iso_scale", "scale"), &VoxelGeneratorHeightmap::set_iso_scale);
	ClassDB::bind_method(D_METHOD("get_iso_scale"), &VoxelGeneratorHeightmap::get_iso_scale);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, gd::VoxelBuffer::CHANNEL_ID_HINT_STRING),
			"set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_start"), "set_height_start", "get_height_start");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_range"), "set_height_range", "get_height_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "iso_scale"), "set_iso_scale", "get_iso_scale");
}

} // namespace zylann::voxel
