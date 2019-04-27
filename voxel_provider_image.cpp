#include "voxel_provider_image.h"

VoxelProviderImage::VoxelProviderImage() :
		_channel(0) {
}

void VoxelProviderImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelProviderImage::get_image() const {
	return _image;
}

void VoxelProviderImage::set_channel(VoxelBuffer::ChannelId channel) {
	_channel = channel;
}

int VoxelProviderImage::get_channel() const {
	return _channel;
}

namespace {

inline int umod(int a, int b) {
	return ((unsigned int)a - (a < 0)) % (unsigned int)b;
}

inline float get_height_repeat(Image &im, int x, int y) {
	return im.get_pixel(umod(x, im.get_width()), umod(y, im.get_height())).r;
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

void VoxelProviderImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels) {

	int ox = origin_in_voxels.x;
	int oy = origin_in_voxels.y;
	int oz = origin_in_voxels.z;

	Image &image = **_image;
	VoxelBuffer &out_buffer = **p_out_buffer;

	image.lock();

	int x = 0;
	int z = 0;

	int bs = out_buffer.get_size().x;

	int dirt = 1;

	while (z < bs) {
		while (x < bs) {

			if (_channel == VoxelBuffer::CHANNEL_ISOLEVEL) {

				float h = get_height_blurred(image, ox + x, oz + z) * 200.0 - 50;

				for (int y = 0; y < bs; ++y) {
					out_buffer.set_voxel_iso((oy + y) - h, x, y, z, _channel);
				}

			} else {
				float h = get_height_repeat(image, ox + x, oz + z) * 200.0 - 50;
				h -= oy;
				int ih = int(h);
				if (ih > 0) {
					if (ih > bs) {
						ih = bs;
					}
					out_buffer.fill_area(dirt, Vector3(x, 0, z), Vector3(x + 1, ih, z + 1), _channel);
				}
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
