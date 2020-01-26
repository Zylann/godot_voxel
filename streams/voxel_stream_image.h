#ifndef HEADER_VOXEL_STREAM_IMAGE
#define HEADER_VOXEL_STREAM_IMAGE

#include "voxel_stream_heightmap.h"
#include <core/image.h>

// Provides infinite tiling heightmap based on an image
class VoxelStreamImage : public VoxelStreamHeightmap {
	GDCLASS(VoxelStreamImage, VoxelStreamHeightmap)
public:
	VoxelStreamImage();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_blur_enabled(bool enable);
	bool is_blur_enabled() const;

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod);

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
	// Mostly here as demo/tweak. It's better recommended to use an EXR/float image.
	bool _blur_enabled = false;
};

#endif // HEADER_VOXEL_STREAM_IMAGE
