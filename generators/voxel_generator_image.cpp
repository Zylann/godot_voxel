#include "voxel_generator_image.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

namespace {

inline float get_height_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

inline float get_height_blurred(Image &im, int x, int y) {
	float h = get_height_repeat(im, x, y);
	h += get_height_repeat(im, x + 1, y);
	h += get_height_repeat(im, x - 1, y);
	h += get_height_repeat(im, x, y + 1);
	h += get_height_repeat(im, x, y - 1);
	return h * 0.2f;
}

} // namespace

VoxelGeneratorImage::VoxelGeneratorImage() {
}

void VoxelGeneratorImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelGeneratorImage::get_image() const {
	return _image;
}

void VoxelGeneratorImage::set_blur_enabled(bool enable) {
	_blur_enabled = enable;
}

bool VoxelGeneratorImage::is_blur_enabled() const {
	return _blur_enabled;
}

void VoxelGeneratorImage::generate_block(VoxelBlockRequest &input) {

	ERR_FAIL_COND(_image.is_null());

	VoxelBuffer &out_buffer = **input.voxel_buffer;
	Image &image = **_image;

	image.lock();

	if (_blur_enabled) {
		VoxelGeneratorHeightmap::generate(out_buffer,
				[&image](int x, int z) { return get_height_blurred(image, x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		VoxelGeneratorHeightmap::generate(out_buffer,
				[&image](int x, int z) { return get_height_repeat(image, x, z); },
				input.origin_in_voxels, input.lod);
	}

	image.unlock();

	out_buffer.compress_uniform_channels();
}

void VoxelGeneratorImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelGeneratorImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelGeneratorImage::get_image);

	ClassDB::bind_method(D_METHOD("set_blur_enabled", "enable"), &VoxelGeneratorImage::set_blur_enabled);
	ClassDB::bind_method(D_METHOD("is_blur_enabled"), &VoxelGeneratorImage::is_blur_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blur_enabled"), "set_blur_enabled", "is_blur_enabled");
}
