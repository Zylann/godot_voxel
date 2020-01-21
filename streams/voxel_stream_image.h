#ifndef HEADER_VOXEL_STREAM_IMAGE
#define HEADER_VOXEL_STREAM_IMAGE

#include "../util/heightmap_sdf.h"
#include "voxel_stream.h"
#include <core/image.h>

// Provides infinite tiling heightmap based on an image
class VoxelStreamImage : public VoxelStream {
	GDCLASS(VoxelStreamImage, VoxelStream)
public:
	VoxelStreamImage();

	enum SdfMode {
		SDF_VERTICAL = HeightmapSdf::SDF_VERTICAL,
		SDF_VERTICAL_AVERAGE = HeightmapSdf::SDF_VERTICAL_AVERAGE,
		SDF_SEGMENT = HeightmapSdf::SDF_SEGMENT,
		SDF_MODE_COUNT = HeightmapSdf::SDF_MODE_COUNT
	};

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_channel(VoxelBuffer::ChannelId channel);
	VoxelBuffer::ChannelId get_channel() const;

	void set_sdf_mode(SdfMode mode);
	SdfMode get_sdf_mode() const;

	void set_height_base(float base);
	float get_height_base() const;

	void set_height_range(float range);
	float get_height_range() const;

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod);

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
	HeightmapSdf _heightmap;
	VoxelBuffer::ChannelId _channel = VoxelBuffer::CHANNEL_TYPE;
	bool _cache_dirty = true;
};

VARIANT_ENUM_CAST(VoxelStreamImage::SdfMode)

#endif // HEADER_VOXEL_STREAM_IMAGE
