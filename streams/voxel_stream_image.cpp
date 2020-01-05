#include "voxel_stream_image.h"
#include "../util/fixed_array.h"

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

float get_constrained_segment_sdf(float p_yp, float p_ya, float p_yb, float p_xb) {

	//  P
	//  .   B
	//  .  /
	//  . /      y
	//  ./       |
	//  A        o--x

	float s = p_yp >= p_ya ? 1 : -1;

	if (Math::absf(p_yp - p_ya) > 1.f && Math::absf(p_yp - p_yb) > 1.f) {
		return s;
	}

	Vector2 p(0, p_yp);
	Vector2 a(0, p_ya);
	Vector2 b(p_xb, p_yb);
	Vector2 closest_point;

	// TODO Optimize given the particular case we are in
	Vector2 n = b - a;
	real_t l2 = n.length_squared();
	if (l2 < 1e-20) {
		closest_point = a; // Both points are the same, just give any.
	} else {
		real_t d = n.dot(p - a) / l2;
		if (d <= 0.0) {
			closest_point = a; // Before first point.
		} else if (d >= 1.0) {
			closest_point = b; // After first point.
		} else {
			closest_point = a + n * d; // Inside.
		}
	}

	return s * closest_point.distance_to(p);
}

} // namespace

const char *VoxelStreamImage::SDF_MODE_HINT_STRING = "Vertical,VerticalAverage,Segment";

VoxelStreamImage::VoxelStreamImage() {
}

void VoxelStreamImage::set_image(Ref<Image> im) {
	_image = im;
}

Ref<Image> VoxelStreamImage::get_image() const {
	return _image;
}

void VoxelStreamImage::set_channel(VoxelBuffer::ChannelId channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_channel = channel;
}

VoxelBuffer::ChannelId VoxelStreamImage::get_channel() const {
	return _channel;
}

void VoxelStreamImage::set_sdf_mode(SdfMode mode) {
	ERR_FAIL_INDEX(mode, SDF_MODE_COUNT);
	_sdf_mode = mode;
}

VoxelStreamImage::SdfMode VoxelStreamImage::get_sdf_mode() const {
	return _sdf_mode;
}

void VoxelStreamImage::emerge_block(Ref<VoxelBuffer> p_out_buffer, Vector3i origin_in_voxels, int lod) {

	int ox = origin_in_voxels.x;
	int oy = origin_in_voxels.y;
	int oz = origin_in_voxels.z;

	Image &image = **_image;
	VoxelBuffer &out_buffer = **p_out_buffer;

	image.lock();

	int bs = out_buffer.get_size().x;

	int dirt = 1;

	float hbase = 50.0;
	float hspan = 200.0;

	for (int z = 0; z < out_buffer.get_size().z; ++z) {
		for (int x = 0; x < out_buffer.get_size().x; ++x) {

			int lx = x << lod;
			int lz = z << lod;

			if (_channel == VoxelBuffer::CHANNEL_TYPE) {

				float h = get_height_repeat(image, ox + lx, oz + lz) * hspan - hbase;
				h -= oy;
				int ih = int(h);
				if (ih > 0) {
					if (ih > bs) {
						ih = bs;
					}
					out_buffer.fill_area(dirt, Vector3(x, 0, z), Vector3(x + 1, ih, z + 1), _channel);
				}

			} else if (_channel == VoxelBuffer::CHANNEL_SDF) {

				switch (_sdf_mode) {
					case SDF_VERTICAL:
					case SDF_VERTICAL_AVERAGE: {
						float h;
						if (_sdf_mode == SDF_VERTICAL) {
							// Only get the height at XZ
							h = get_height_repeat(image, ox + lx, oz + lz) * hspan - hbase;
						} else {
							// Get an average of the heights around XZ
							h = get_height_blurred(image, ox + lx, oz + lz) * hspan - hbase;
						}
						for (int y = 0; y < out_buffer.get_size().y; ++y) {

							int ly = y << lod;
							float d = (oy + ly) - h;
							out_buffer.set_voxel_f(d, x, y, z, _channel);
						}
					} break;

					case SDF_SEGMENT: {
						// Calculate distance to 8 segments going from the point at XZ to its neighbor points,
						// and pick the smallest distance.

						int gx = ox + lx;
						int gz = oz + lz;

						float h0 = get_height_repeat(image, gx - 1, gz - 1) * hspan - hbase;
						float h1 = get_height_repeat(image, gx, gz - 1) * hspan - hbase;
						float h2 = get_height_repeat(image, gx + 1, gz - 1) * hspan - hbase;

						float h3 = get_height_repeat(image, gx - 1, gz) * hspan - hbase;
						float h4 = get_height_repeat(image, gx, gz) * hspan - hbase;
						float h5 = get_height_repeat(image, gx + 1, gz) * hspan - hbase;

						float h6 = get_height_repeat(image, gx - 1, gz + 1) * hspan - hbase;
						float h7 = get_height_repeat(image, gx, gz + 1) * hspan - hbase;
						float h8 = get_height_repeat(image, gx + 1, gz + 1) * hspan - hbase;

						const float sqrt2 = 1.414213562373095;

						for (int y = 0; y < out_buffer.get_size().y; ++y) {

							int ly = y << lod;
							int gy = oy + ly;

							float sdf0 = get_constrained_segment_sdf(gy, h4, h0, sqrt2);
							float sdf1 = get_constrained_segment_sdf(gy, h4, h1, 1);
							float sdf2 = get_constrained_segment_sdf(gy, h4, h2, sqrt2);

							float sdf3 = get_constrained_segment_sdf(gy, h4, h3, 1);
							float sdf4 = gy - h4;
							float sdf5 = get_constrained_segment_sdf(gy, h4, h5, 1);

							float sdf6 = get_constrained_segment_sdf(gy, h4, h6, sqrt2);
							float sdf7 = get_constrained_segment_sdf(gy, h4, h7, 1);
							float sdf8 = get_constrained_segment_sdf(gy, h4, h8, sqrt2);

							float sdf = sdf4;

							if (Math::absf(sdf0) < Math::absf(sdf)) {
								sdf = sdf0;
							}
							if (Math::absf(sdf1) < Math::absf(sdf)) {
								sdf = sdf1;
							}
							if (Math::absf(sdf2) < Math::absf(sdf)) {
								sdf = sdf2;
							}
							if (Math::absf(sdf3) < Math::absf(sdf)) {
								sdf = sdf3;
							}
							if (Math::absf(sdf5) < Math::absf(sdf)) {
								sdf = sdf5;
							}
							if (Math::absf(sdf6) < Math::absf(sdf)) {
								sdf = sdf6;
							}
							if (Math::absf(sdf7) < Math::absf(sdf)) {
								sdf = sdf7;
							}
							if (Math::absf(sdf8) < Math::absf(sdf)) {
								sdf = sdf8;
							}

							out_buffer.set_voxel_f(sdf, x, y, z, _channel);
						}
					} break;

					default:
						CRASH_NOW();
						break;

				} // sdf mode

			} // channel

		} // for x
	} // for z

	image.unlock();

	out_buffer.compress_uniform_channels();
}

void VoxelStreamImage::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_image", "image"), &VoxelStreamImage::set_image);
	ClassDB::bind_method(D_METHOD("get_image"), &VoxelStreamImage::get_image);

	ClassDB::bind_method(D_METHOD("set_channel", "channel"), &VoxelStreamImage::set_channel);
	ClassDB::bind_method(D_METHOD("get_channel"), &VoxelStreamImage::get_channel);

	ClassDB::bind_method(D_METHOD("set_sdf_mode", "mode"), &VoxelStreamImage::set_sdf_mode);
	ClassDB::bind_method(D_METHOD("get_sdf_mode"), &VoxelStreamImage::get_sdf_mode);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image"), "set_image", "get_image");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "channel", PROPERTY_HINT_ENUM, VoxelBuffer::CHANNEL_ID_HINT_STRING), "set_channel", "get_channel");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sdf_mode", PROPERTY_HINT_ENUM, SDF_MODE_HINT_STRING), "set_sdf_mode", "get_sdf_mode");

	BIND_ENUM_CONSTANT(SDF_VERTICAL);
	BIND_ENUM_CONSTANT(SDF_VERTICAL_AVERAGE);
	BIND_ENUM_CONSTANT(SDF_SEGMENT);
}
