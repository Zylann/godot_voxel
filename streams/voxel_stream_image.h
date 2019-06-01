#ifndef HEADER_VOXEL_STREAM_IMAGE
#define HEADER_VOXEL_STREAM_IMAGE

#include "voxel_stream.h"
#include <core/image.h>

// Provides infinite tiling heightmap based on an image
class VoxelStreamImage : public VoxelStream {
	GDCLASS(VoxelStreamImage, VoxelStream)
public:
	VoxelStreamImage();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod);

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
	VoxelBuffer::ChannelId _channel;
};

#endif // HEADER_VOXEL_STREAM_IMAGE
