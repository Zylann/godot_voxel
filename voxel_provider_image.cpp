#include "voxel_provider_image.h"

VoxelProviderImage::VoxelProviderImage(): _channel(0) {
}

void VoxelProviderImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelProviderImage::get_image() const {
	return _image;
}

void VoxelProviderImage::set_channel(int channel) {
	_channel = channel;
}

int VoxelProviderImage::get_channel() const {
	return _channel;
}

void VoxelProviderImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels) {

	int ox = origin_in_voxels.x;
	int oy = origin_in_voxels.y;
	int oz = origin_in_voxels.z;

	Image &image = **_image;
	VoxelBuffer &out_buffer = **p_out_buffer;

	image.lock();

	int im_w = image.get_width();
	int im_h = image.get_height();
	int im_wm = im_w - 1;
	int im_hm = im_h - 1;

	int x = 0;
	int z = 0;

	int bs = out_buffer.get_size().x;

	int dirt = 1;

	while (z < bs) {
		while (x < bs) {

			Color c = image.get_pixel((ox + x) & im_wm, (oz + z) & im_hm);
			int h = int(c.r * 200.0) - 50;
			h -= oy;
			if (h > 0) {
				if (h > bs) {
					h = bs;
				}
				out_buffer.fill_area(dirt, Vector3(x, 0, z), Vector3(x + 1, h, z + 1), _channel);
			}

			x += 1;
		}
		z += 1;
		x = 0;
	}

	image.unlock();
}

void VoxelProviderImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelProviderImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelProviderImage::get_image);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelProviderImage::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelProviderImage::get_channel);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel"), "set_channel", "get_channel");
}


