#ifndef HEADER_VOXEL_GENERATOR_IMAGE
#define HEADER_VOXEL_GENERATOR_IMAGE

#include "voxel_generator_heightmap.h"
#include <core/image.h>

// Provides infinite tiling heightmap based on an image
class VoxelGeneratorImage : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorImage, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorImage();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_blur_enabled(bool enable);
	bool is_blur_enabled() const;

	void generate_block(VoxelBlockRequest &input) override;

private:
	static void _bind_methods();

private:
	Ref<Image> _image;
	// Mostly here as demo/tweak. It's better recommended to use an EXR/float image.
	bool _blur_enabled = false;
};

#endif // HEADER_VOXEL_GENERATOR_IMAGE
