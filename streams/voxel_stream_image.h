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

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod);

	void get_single_sdf(Vector3i position, float &value) override;

	bool is_cloneable() const override;
	Ref<Resource> duplicate(bool p_subresources) const override;

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
};

#endif // HEADER_VOXEL_STREAM_IMAGE
