#ifndef HEADER_VOXEL_GENERATOR_IMAGE
#define HEADER_VOXEL_GENERATOR_IMAGE

#include "../../util/godot/classes/image.h"
#include "voxel_generator_heightmap.h"

namespace zylann::voxel {

// Provides infinite tiling heightmap based on an image
class VoxelGeneratorImage : public VoxelGeneratorHeightmap {
	GDCLASS(VoxelGeneratorImage, VoxelGeneratorHeightmap)

public:
	VoxelGeneratorImage();
	~VoxelGeneratorImage();

	void set_image(Ref<Image> im);
	Ref<Image> get_image() const;

	void set_blur_enabled(bool enable);
	bool is_blur_enabled() const;

	Result generate_block(VoxelGenerator::VoxelQueryData &input) override;

private:
	static void _bind_methods();

private:
	// Proper reference used for external access.
	Ref<Image> _image;

	struct Parameters {
		// This is a read-only copy of the image.
		// It wastes memory for sure, but Godot does not offer any way to secure this better.
		// If this is a problem one day, we could add an option to dereference the external image in game.
		Ref<Image> image;
		// Mostly here as demo/tweak. It's better recommended to use an EXR/float image.
		bool blur_enabled = false;
	};

	Parameters _parameters;
	RWLock _parameters_lock;
};

} // namespace zylann::voxel

#endif // HEADER_VOXEL_GENERATOR_IMAGE
