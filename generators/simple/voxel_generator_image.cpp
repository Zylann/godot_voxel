#include "voxel_generator_image.h"
#include "../../util/fixed_array.h"
#include "../../util/span.h"

namespace {

inline float get_height_repeat(const Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

inline float get_height_blurred(const Image &im, int x, int y) {
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

VoxelGeneratorImage::~VoxelGeneratorImage() {
	if (_parameters.image.is_valid()) {
		_parameters.image->unlock();
	}
}

void VoxelGeneratorImage::set_image(Ref<Image> im) {
	if (im == _image) {
		return;
	}
	_image = im;
	Ref<Image> copy;
	if (im.is_valid()) {
		copy = im->duplicate();
	}
	RWLockWrite wlock(_parameters_lock);
	// lock() prevents us from reading the same image from multiple threads, so we lock it up-front.
	// This might no longer be needed in Godot 4.
	if (_parameters.image.is_valid()) {
		_parameters.image->unlock();
	}
	_parameters.image = copy;
	if (_parameters.image.is_valid()) {
		_parameters.image->lock();
	}
}

Ref<Image> VoxelGeneratorImage::get_image() const {
	return _image;
}

void VoxelGeneratorImage::set_blur_enabled(bool enable) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.blur_enabled = enable;
}

bool VoxelGeneratorImage::is_blur_enabled() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.blur_enabled;
}

VoxelGenerator::Result VoxelGeneratorImage::generate_block(VoxelBlockRequest &input) {
	VoxelBufferInternal &out_buffer = input.voxel_buffer;

	Parameters params;
	{
		RWLockRead rlock(_parameters_lock);
		params = _parameters;
	}

	Result result;

	ERR_FAIL_COND_V(params.image.is_null(), result);
	const Image &image = **params.image;

	if (params.blur_enabled) {
		result = VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&image](int x, int z) { return get_height_blurred(image, x, z); },
				input.origin_in_voxels, input.lod);
	} else {
		result = VoxelGeneratorHeightmap::generate(
				out_buffer,
				[&image](int x, int z) { return get_height_repeat(image, x, z); },
				input.origin_in_voxels, input.lod);
	}

	out_buffer.compress_uniform_channels();
	return result;
}

void VoxelGeneratorImage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelGeneratorImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelGeneratorImage::get_image);

	ClassDB::bind_method(D_METHOD("set_blur_enabled", "enable"), &VoxelGeneratorImage::set_blur_enabled);
	ClassDB::bind_method(D_METHOD("is_blur_enabled"), &VoxelGeneratorImage::is_blur_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "blur_enabled"), "set_blur_enabled", "is_blur_enabled");
}
