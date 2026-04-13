#include "voxel_format_gd.h"
#include "../constants/voxel_string_names.h"
#include "../util/string/format.h"

#ifdef ZN_GODOT
#include "../util/godot/core/class_db.h"
#endif

namespace zylann::voxel::godot {

void VoxelFormat::set_channel_depth(const VoxelBuffer::ChannelId channel_index, const VoxelBuffer::Depth depth) {
	ZN_ASSERT_RETURN(channel_index >= 0 && channel_index < _internal.depths.size());

	zylann::voxel::VoxelBuffer::Depth idepth = static_cast<zylann::voxel::VoxelBuffer::Depth>(depth);

	zylann::voxel::VoxelBuffer::ChannelId ichannel_index =
			static_cast<zylann::voxel::VoxelBuffer::ChannelId>(channel_index);
	const zylann::voxel::VoxelFormat::DepthRange supported_range =
			zylann::voxel::VoxelFormat::get_supported_depths(ichannel_index);

	const unsigned int byte_count = zylann::voxel::VoxelBuffer::get_depth_byte_count(idepth);
	if (!supported_range.contains(byte_count)) {
		ZN_PRINT_ERROR(
				format("Depth of {}-bits is not supported by channel `{}`",
					   byte_count,
					   zylann::voxel::VoxelBuffer::get_channel_name(ichannel_index))
		);
		return;
	}

	if (_internal.depths[channel_index] == idepth) {
		return;
	}
	_internal.depths[channel_index] = idepth;
	emit_changed();
}

VoxelBuffer::Depth VoxelFormat::get_channel_depth(const VoxelBuffer::ChannelId channel_index) const {
	ZN_ASSERT_RETURN_V(channel_index >= 0 && channel_index < _internal.depths.size(), VoxelBuffer::DEPTH_COUNT);
	return static_cast<VoxelBuffer::Depth>(_internal.depths[channel_index]);
}

void VoxelFormat::configure_buffer(Ref<VoxelBuffer> buffer) const {
	ZN_ASSERT_RETURN(buffer.is_valid());
	_internal.configure_buffer(buffer->get_buffer());
}

Ref<VoxelBuffer> VoxelFormat::create_buffer(const Vector3i size) const {
	Ref<VoxelBuffer> buffer;
	ZN_ASSERT_RETURN_V(Vector3iUtil::is_valid_size(size), buffer);
	buffer.instantiate();
	_internal.configure_buffer(buffer->get_buffer());
	if (size != Vector3i()) {
		buffer->create(size.x, size.y, size.z);
	}
	return buffer;
}

void VoxelFormat::_b_set_data(const Array &data) {
	ZN_ASSERT_RETURN(data.size() >= 1);
	const int version = data[0];
	ZN_ASSERT_RETURN(version == 0);

	ZN_ASSERT_RETURN(data.size() == 9);
	for (unsigned int channel_index = 0; channel_index < _internal.depths.size(); ++channel_index) {
		const int depth = data[1 + channel_index];
		ZN_ASSERT_CONTINUE(depth >= 0 && depth < VoxelBuffer::DEPTH_COUNT);
		_internal.depths[channel_index] = static_cast<zylann::voxel::VoxelBuffer::Depth>(depth);
	}
}

Array VoxelFormat::_b_get_data() const {
	Array data;
	data.resize(9);
	data[0] = 0;

	for (unsigned int channel_index = 0; channel_index < _internal.depths.size(); ++channel_index) {
		const int depth = _internal.depths[channel_index];
		data[1 + channel_index] = depth;
	}

	return data;
}

void VoxelFormat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_channel_depth", "channel_index", "depth"), &VoxelFormat::set_channel_depth);
	ClassDB::bind_method(D_METHOD("get_channel_depth", "channel_index"), &VoxelFormat::get_channel_depth);

	ClassDB::bind_method(D_METHOD("configure_buffer", "buffer"), &VoxelFormat::configure_buffer);
	ClassDB::bind_method(D_METHOD("create_buffer", "size"), &VoxelFormat::create_buffer);

	ClassDB::bind_method(D_METHOD("_set_data", "data"), &VoxelFormat::_b_set_data);
	ClassDB::bind_method(D_METHOD("_get_data"), &VoxelFormat::_b_get_data);

	ADD_PROPERTY(
			PropertyInfo(Variant::ARRAY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE),
			"_set_data",
			"_get_data"
	);

	const String depth_hint_string = "8bit,16bit,32bit";

	ADD_PROPERTYI(
			PropertyInfo(Variant::INT, "type_depth", PROPERTY_HINT_ENUM, depth_hint_string, PROPERTY_USAGE_EDITOR),
			"set_channel_depth",
			"get_channel_depth",
			VoxelBuffer::CHANNEL_TYPE
	);
	ADD_PROPERTYI(
			PropertyInfo(Variant::INT, "sdf_depth", PROPERTY_HINT_ENUM, depth_hint_string, PROPERTY_USAGE_EDITOR),
			"set_channel_depth",
			"get_channel_depth",
			VoxelBuffer::CHANNEL_SDF
	);
	ADD_PROPERTYI(
			PropertyInfo(Variant::INT, "indices_depth", PROPERTY_HINT_ENUM, depth_hint_string, PROPERTY_USAGE_EDITOR),
			"set_channel_depth",
			"get_channel_depth",
			VoxelBuffer::CHANNEL_INDICES
	);
	ADD_PROPERTYI(
			PropertyInfo(Variant::INT, "color_depth", PROPERTY_HINT_ENUM, depth_hint_string, PROPERTY_USAGE_EDITOR),
			"set_channel_depth",
			"get_channel_depth",
			VoxelBuffer::CHANNEL_COLOR
	);
}

} // namespace zylann::voxel::godot
