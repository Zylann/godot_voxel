#include "voxel_buffer.h"
#include "voxel_tool_buffer.h"

#include <core/math/math_funcs.h>
#include <string.h>

const char *VoxelBuffer::CHANNEL_ID_HINT_STRING = "Type,Sdf,Data2,Data3,Data4,Data5,Data6,Data7";

VoxelBuffer::VoxelBuffer() {
	_channels[CHANNEL_ISOLEVEL].defval = 255;
}

VoxelBuffer::~VoxelBuffer() {
	clear();
}

void VoxelBuffer::create(int sx, int sy, int sz) {
	if (sx <= 0 || sy <= 0 || sz <= 0) {
		return;
	}
	Vector3i new_size(sx, sy, sz);
	if (new_size != _size) {
		for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
			Channel &channel = _channels[i];
			if (channel.data) {
				// Channel already contained data
				// TODO Optimize with realloc
				delete_channel(i);
				create_channel(i, new_size, channel.defval);
			}
		}
		_size = new_size;
	}
}

void VoxelBuffer::create(Vector3i size) {
	create(size.x, size.y, size.z);
}

void VoxelBuffer::clear() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		Channel &channel = _channels[i];
		if (channel.data) {
			delete_channel(i);
		}
	}
}

void VoxelBuffer::clear_channel(unsigned int channel_index, int clear_value) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	if (_channels[channel_index].data) {
		delete_channel(channel_index);
	}
	_channels[channel_index].defval = clear_value;
}

void VoxelBuffer::set_default_values(uint8_t values[VoxelBuffer::MAX_CHANNELS]) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		_channels[i].defval = values[i];
	}
}

int VoxelBuffer::get_voxel(int x, int y, int z, unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, 0);

	const Channel &channel = _channels[channel_index];

	if (validate_pos(x, y, z) && channel.data) {
		return channel.data[index(x, y, z)];
	} else {
		return channel.defval;
	}
}

void VoxelBuffer::set_voxel(int value, int x, int y, int z, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND(!validate_pos(x, y, z));

	Channel &channel = _channels[channel_index];

	if (channel.data == NULL) {
		if (channel.defval != value) {
			// Allocate channel with same initial values as defval
			create_channel(channel_index, _size, channel.defval);
			channel.data[index(x, y, z)] = value;
		}
	} else {
		channel.data[index(x, y, z)] = value;
	}
}

// This version does not cause errors if out of bounds. Use only if it's okay to be outside.
void VoxelBuffer::try_set_voxel(int x, int y, int z, int value, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	if (!validate_pos(x, y, z)) {
		return;
	}

	Channel &channel = _channels[channel_index];

	if (channel.data == NULL) {
		if (channel.defval != value) {
			create_channel(channel_index, _size, channel.defval);
			channel.data[index(x, y, z)] = value;
		}
	} else {
		channel.data[index(x, y, z)] = value;
	}
}

void VoxelBuffer::fill(int defval, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Channel &channel = _channels[channel_index];
	if (channel.data == NULL) {
		// Channel is already optimized and uniform
		if (channel.defval == defval) {
			// No change
			return;
		} else {
			// Just change default value
			channel.defval = defval;
			return;
		}
	} else {
		create_channel_noinit(channel_index, _size);
	}

	unsigned int volume = get_volume();
	memset(channel.data, defval, volume);
}

void VoxelBuffer::fill_area(int defval, Vector3i min, Vector3i max, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Vector3i::sort_min_max(min, max);

	min.clamp_to(Vector3i(0, 0, 0), _size + Vector3i(1, 1, 1));
	max.clamp_to(Vector3i(0, 0, 0), _size + Vector3i(1, 1, 1));
	Vector3i area_size = max - min;

	if (area_size.x == 0 || area_size.y == 0 || area_size.z == 0) {
		return;
	}

	Channel &channel = _channels[channel_index];
	if (channel.data == NULL) {
		if (channel.defval == defval) {
			return;
		} else {
			create_channel(channel_index, _size, channel.defval);
		}
	}

	Vector3i pos;
	unsigned int volume = get_volume();
	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
		for (pos.x = min.x; pos.x < max.x; ++pos.x) {
			unsigned int dst_ri = index(pos.x, pos.y + min.y, pos.z);
			CRASH_COND(dst_ri >= volume);
			memset(&channel.data[dst_ri], defval, area_size.y * sizeof(uint8_t));
		}
	}
}

bool VoxelBuffer::is_uniform(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);

	const Channel &channel = _channels[channel_index];
	if (channel.data == NULL) {
		// Channel has been optimized
		return true;
	}

	// Channel isn't optimized, so must look at each voxel
	uint8_t voxel = channel.data[0];
	unsigned int volume = get_volume();
	for (unsigned int i = 1; i < volume; ++i) {
		if (channel.data[i] != voxel) {
			return false;
		}
	}

	return true;
}

void VoxelBuffer::compress_uniform_channels() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		if (_channels[i].data && is_uniform(i)) {
			clear_channel(i, _channels[i].data[0]);
		}
	}
}

void VoxelBuffer::decompress_channel(unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		create_channel(channel_index, _size, channel.defval);
	}
}

VoxelBuffer::Compression VoxelBuffer::get_channel_compression(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, VoxelBuffer::COMPRESSION_NONE);
	const Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		return COMPRESSION_UNIFORM;
	}
	return COMPRESSION_NONE;
}

void VoxelBuffer::grab_channel_data(uint8_t *in_buffer, unsigned int channel_index, Compression compression) {
	CRASH_COND(channel_index >= MAX_CHANNELS);

	// Only case supported at the moment
	CRASH_COND(in_buffer == nullptr);
	CRASH_COND(compression != COMPRESSION_NONE);

	Channel &channel = _channels[channel_index];
	if (channel.data) {
		delete_channel(channel_index);
	}
	// Takes ownership of the provided buffer
	channel.data = in_buffer;
}

void VoxelBuffer::copy_from(const VoxelBuffer &other, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND(other._size != _size);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	if (other_channel.data) {
		if (channel.data == NULL) {
			create_channel_noinit(channel_index, _size);
		}
		memcpy(channel.data, other_channel.data, get_volume() * sizeof(uint8_t));
	} else if (channel.data) {
		delete_channel(channel_index);
	}

	channel.defval = other_channel.defval;
}

void VoxelBuffer::copy_from(const VoxelBuffer &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min, unsigned int channel_index) {

	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	if (channel.data == nullptr && other_channel.data == nullptr && channel.defval == other_channel.defval) {
		// No action needed
		return;
	}

	Vector3i::sort_min_max(src_min, src_max);

	src_min.clamp_to(Vector3i(0, 0, 0), other._size);
	src_max.clamp_to(Vector3i(0, 0, 0), other._size + Vector3i(1, 1, 1));

	dst_min.clamp_to(Vector3i(0, 0, 0), _size);
	Vector3i area_size = src_max - src_min;
	//Vector3i dst_max = dst_min + area_size;

	if (area_size == _size && area_size == other._size) {
		// Equivalent of full copy between two blocks of same size
		copy_from(other, channel_index);

	} else {
		if (other_channel.data) {
			if (channel.data == NULL) {
				create_channel(channel_index, _size, channel.defval);
			}
			// Copy row by row
			Vector3i pos;
			for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
				for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
					// Row direction is Y
					unsigned int src_ri = other.index(pos.x + src_min.x, pos.y + src_min.y, pos.z + src_min.z);
					unsigned int dst_ri = index(pos.x + dst_min.x, pos.y + dst_min.y, pos.z + dst_min.z);
					memcpy(&channel.data[dst_ri], &other_channel.data[src_ri], area_size.y * sizeof(uint8_t));
				}
			}
		} else if (channel.defval != other_channel.defval) {
			if (channel.data == NULL) {
				create_channel(channel_index, _size, channel.defval);
			}
			// Set row by row
			Vector3i pos;
			for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
				for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
					unsigned int dst_ri = index(pos.x + dst_min.x, pos.y + dst_min.y, pos.z + dst_min.z);
					memset(&channel.data[dst_ri], other_channel.defval, area_size.y * sizeof(uint8_t));
				}
			}
		}
	}
}

Ref<VoxelBuffer> VoxelBuffer::duplicate() const {
	VoxelBuffer *d = memnew(VoxelBuffer);
	d->create(_size);
	d->copy_from(*this);
	return Ref<VoxelBuffer>(d);
}

uint8_t *VoxelBuffer::get_channel_raw(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, NULL);
	const Channel &channel = _channels[channel_index];
	return channel.data;
}

void VoxelBuffer::create_channel(int i, Vector3i size, uint8_t defval) {
	create_channel_noinit(i, size);
	memset(_channels[i].data, defval, get_volume() * sizeof(uint8_t));
}

void VoxelBuffer::create_channel_noinit(int i, Vector3i size) {
	Channel &channel = _channels[i];
	unsigned int volume = size.x * size.y * size.z;
	channel.data = (uint8_t *)memalloc(volume * sizeof(uint8_t));
}

void VoxelBuffer::delete_channel(int i) {
	Channel &channel = _channels[i];
	ERR_FAIL_COND(channel.data == NULL);
	memfree(channel.data);
	channel.data = NULL;
}

void VoxelBuffer::downscale_to(VoxelBuffer &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const {

	// TODO Align input to multiple of two

	src_min.clamp_to(Vector3i(), _size);
	src_max.clamp_to(Vector3i(), _size + Vector3i(1));

	Vector3i dst_max = dst_min + ((src_max - src_min) >> 1);

	dst_min.clamp_to(Vector3i(), dst._size);
	dst_max.clamp_to(Vector3i(), dst._size + Vector3i(1));

	for (int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {

		const Channel &src_channel = _channels[channel_index];
		const Channel &dst_channel = dst._channels[channel_index];

		if (src_channel.data == nullptr && dst_channel.data == nullptr && src_channel.defval == dst_channel.defval) {
			// No action needed
			continue;
		}

		// Nearest-neighbor downscaling

		Vector3i pos;
		for (pos.z = dst_min.z; pos.z < dst_max.z; ++pos.z) {
			for (pos.x = dst_min.x; pos.x < dst_max.x; ++pos.x) {
				for (pos.y = dst_min.y; pos.y < dst_max.y; ++pos.y) {

					Vector3i src_pos = src_min + ((pos - dst_min) << 1);

					// TODO Remove check once it works
					CRASH_COND(!validate_pos(src_pos.x, src_pos.y, src_pos.z));

					int v;
					if (src_channel.data) {
						v = src_channel.data[index(src_pos.x, src_pos.y, src_pos.z)];
					} else {
						v = src_channel.defval;
					}

					dst.set_voxel(v, pos, channel_index);
				}
			}
		}
	}
}

Ref<VoxelTool> VoxelBuffer::get_voxel_tool() const {
	return Ref<VoxelTool>(memnew(VoxelToolBuffer(Ref<VoxelBuffer>(this))));
}

bool VoxelBuffer::equals(const VoxelBuffer *p_other) const {
	CRASH_COND(p_other == nullptr);

	if (p_other->_size != _size) {
		return false;
	}

	for (int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {

		const Channel &channel = _channels[channel_index];
		const Channel &other_channel = p_other->_channels[channel_index];

		if ((channel.data == nullptr) != (other_channel.data == nullptr)) {
			// Note: they could still logically be equal if one channel contains uniform voxel memory
			return false;
		}

		if (channel.data == nullptr) {
			if (channel.defval != other_channel.defval) {
				return false;
			}

		} else {
			unsigned int volume = _size.volume();
			for (unsigned int i = 0; i < volume; ++i) {
				if (channel.data[i] != other_channel.data[i]) {
					return false;
				}
			}
		}
	}

	return true;
}

#ifdef TOOLS_ENABLED

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down() {
	Image *im = memnew(Image);
	im->create(_size.x, _size.z, false, Image::FORMAT_RGB8);
	im->lock();
	Vector3i pos;
	for (pos.z = 0; pos.z < _size.z; ++pos.z) {
		for (pos.x = 0; pos.x < _size.x; ++pos.x) {
			for (pos.y = _size.y - 1; pos.y >= 0; --pos.y) {
				float v = get_voxel_f(pos.x, pos.y, pos.z, CHANNEL_ISOLEVEL);
				if (v < 0.0) {
					break;
				}
			}
			float h = pos.y;
			float c = h / _size.y;
			im->set_pixel(pos.x, pos.z, Color(c, c, c));
		}
	}
	im->unlock();
	return Ref<Image>(im);
}

#endif

void VoxelBuffer::_bind_methods() {

	ClassDB::bind_method(D_METHOD("create", "sx", "sy", "sz"), &VoxelBuffer::_b_create);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBuffer::clear);

	ClassDB::bind_method(D_METHOD("get_size"), &VoxelBuffer::_b_get_size);
	ClassDB::bind_method(D_METHOD("get_size_x"), &VoxelBuffer::get_size_x);
	ClassDB::bind_method(D_METHOD("get_size_y"), &VoxelBuffer::get_size_y);
	ClassDB::bind_method(D_METHOD("get_size_z"), &VoxelBuffer::get_size_z);

	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "channel"), &VoxelBuffer::_b_set_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_f", "value", "x", "y", "z", "channel"), &VoxelBuffer::_b_set_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::_b_set_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::_b_get_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_f", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel_f, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_f", "value", "channel"), &VoxelBuffer::fill_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area", "value", "min", "max", "channel"), &VoxelBuffer::_b_fill_area, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("copy_from", "other", "channel"), &VoxelBuffer::_b_copy_from, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("copy_from_area", "other", "src_min", "src_max", "dst_min", "channel"), &VoxelBuffer::_b_copy_from_area, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("is_uniform", "channel"), &VoxelBuffer::is_uniform);
	ClassDB::bind_method(D_METHOD("optimize"), &VoxelBuffer::compress_uniform_channels);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE);
	BIND_ENUM_CONSTANT(CHANNEL_ISOLEVEL);
	BIND_ENUM_CONSTANT(CHANNEL_DATA2);
	BIND_ENUM_CONSTANT(CHANNEL_DATA3);
	BIND_ENUM_CONSTANT(CHANNEL_DATA4);
	BIND_ENUM_CONSTANT(CHANNEL_DATA5);
	BIND_ENUM_CONSTANT(CHANNEL_DATA6);
	BIND_ENUM_CONSTANT(CHANNEL_DATA7);
	BIND_ENUM_CONSTANT(MAX_CHANNELS);
}

void VoxelBuffer::_b_copy_from(Ref<VoxelBuffer> other, unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	copy_from(**other, channel);
}

void VoxelBuffer::_b_copy_from_area(Ref<VoxelBuffer> other, Vector3 src_min, Vector3 src_max, Vector3 dst_min, unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	copy_from(**other, Vector3i(src_min), Vector3i(src_max), Vector3i(dst_min), channel);
}
