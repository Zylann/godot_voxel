#include "voxel_stream_image.h"

VoxelStreamImage::VoxelStreamImage() :
		_channel(VoxelBuffer::CHANNEL_TYPE) {
}

void VoxelStreamImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelStreamImage::get_image() const {
	return _image;
}

void VoxelStreamImage::set_channel(VoxelBuffer::ChannelId channel) {
	_channel = channel;
}

VoxelBuffer::ChannelId VoxelStreamImage::get_channel() const {
	return _channel;
}

namespace {

inline int wrap(int a, int b) {
	return ((unsigned int)a - (a < 0)) % (unsigned int)b;
}

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

void VoxelStreamImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod) {

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

	float hbase = 50.0;
	float hspan = 200.0;

	while (z < bs) {
		while (x < bs) {

			int lx = x << lod;
			int lz = z << lod;

			if (_channel == VoxelBuffer::CHANNEL_ISOLEVEL) {

				float h = get_height_blurred(image, ox + lx, oz + lz) * hspan - hbase;

				for (int y = 0; y < bs; ++y) {
					int ly = y << lod;
					out_buffer.set_voxel_f((oy + ly) - h, x, y, z, _channel);
				}

			} else {
				float h = get_height_repeat(image, ox + lx, oz + lz) * hspan - hbase;
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

void VoxelStreamImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelStreamImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelStreamImage::get_image);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamImage::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamImage::get_channel);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
}
