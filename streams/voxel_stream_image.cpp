#include "voxel_stream_image.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"

namespace {

inline float get_height_repeat(Image &im, int x, int y) {
	return im.get_pixel(wrap(x, im.get_width()), wrap(y, im.get_height())).r;
}

} // namespace

VoxelStreamImage::VoxelStreamImage() {
}

void VoxelStreamImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelStreamImage::get_image() const {
	return _image;
}

void VoxelStreamImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod) {

	ERR_FAIL_COND(_image.is_null());

	VoxelBuffer &out_buffer = **p_out_buffer;
	Image &image = **_image;

	image.lock();

	VoxelStreamHeightmap::generate(out_buffer,
			[&image](int x, int z) { return get_height_repeat(image, x, z); },
			origin_in_voxels.x, origin_in_voxels.y, origin_in_voxels.z, lod);

	image.unlock();

	// TODO TESTING
	//	for (int z = 0; z < out_buffer.get_size().z; ++z) {
	//		for (int x = 0; x < out_buffer.get_size().x; ++x) {
	//			for (int y = 0; y < out_buffer.get_size().y; ++y) {
	//				Vector3i p = origin_in_voxels + (Vector3i(x, y, z) << lod);
	//				float sdf;
	//				get_single_sdf(p, sdf);
	//				out_buffer.set_voxel_f(sdf, x, y, z, get_channel());
	//			}
	//		}
	//	}

	out_buffer.compress_uniform_channels();
}

void VoxelStreamImage::get_single_sdf(Vector3i position, float &value) {

	ERR_FAIL_COND(_image.is_null());

	Image &image = **_image;

	image.lock();

	VoxelStreamHeightmap::generate_single_sdf(value,
			[&image](int x, int z) { return get_height_repeat(image, x, z); },
			position.x, position.y, position.z);

	image.unlock();
}

bool VoxelStreamImage::is_cloneable() const {
	return true;
}

Ref<Resource> VoxelStreamImage::duplicate(bool p_subresources) const {
	Ref<VoxelStreamImage> d;
	d.instance();
	if (_image.is_valid()) {
		Ref<Image> image = _image;
		if (p_subresources) {
			image = image->duplicate();
		}
		d->set_image(image);
	}
	d->copy_heightmap_settings_from(*this);
	return d;
}

void VoxelStreamImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelStreamImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelStreamImage::get_image);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
}
