#ifndef VOXEL_FORMAT_GD_H
#define VOXEL_FORMAT_GD_H

#include "../util/godot/classes/resource.h"
#include "voxel_buffer_gd.h"
#include "voxel_format.h"

namespace zylann::voxel::godot {

class VoxelFormat : public Resource {
	GDCLASS(VoxelFormat, Resource)
public:
	void set_channel_depth(const VoxelBuffer::ChannelId channel_index, const VoxelBuffer::Depth depth);
	VoxelBuffer::Depth get_channel_depth(const VoxelBuffer::ChannelId channel_index) const;

	void configure_buffer(Ref<VoxelBuffer> buffer) const;
	Ref<VoxelBuffer> create_buffer(const Vector3i size) const;

	zylann::voxel::VoxelFormat get_internal() const {
		return _internal;
	}

private:
	void _b_set_data(const Array &data);
	Array _b_get_data() const;

	static void _bind_methods();

	zylann::voxel::VoxelFormat _internal;
};

} // namespace zylann::voxel::godot

#endif // VOXEL_FORMAT_GD_H
