#ifndef HEADER_VOXEL_PROVIDER_IMAGE
#define HEADER_VOXEL_PROVIDER_IMAGE

#include "voxel_provider.h"
#include <core/image.h>

// TODO Rename VoxelProviderHeightmap
// Provides infinite tiling heightmap based on an image
class VoxelProviderImage : public VoxelProvider {
	GDCLASS(VoxelProviderImage, VoxelProvider)
public:
	VoxelProviderImage();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_channel(int channel);
	int get_channel() const;

	void emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels);

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
	int _channel;
};

#endif // HEADER_VOXEL_PROVIDER_IMAGE
