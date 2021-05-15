#define VOXEL_BUFFER_USE_MEMORY_POOL

#ifdef VOXEL_BUFFER_USE_MEMORY_POOL
#include "voxel_memory_pool.h"
#endif

#include "../edition/voxel_tool_buffer.h"
#include "../util/funcs.h"
#include "../util/profiling.h"
#include "voxel_buffer.h"

#include <core/func_ref.h>
#include <core/image.h>
#include <core/io/marshalls.h>
#include <core/math/math_funcs.h>
#include <string.h>

namespace {

inline uint8_t *allocate_channel_data(uint32_t size) {
#ifdef VOXEL_BUFFER_USE_MEMORY_POOL
	return VoxelMemoryPool::get_singleton()->allocate(size);
#else
	return (uint8_t *)memalloc(size * sizeof(uint8_t));
#endif
}

inline void free_channel_data(uint8_t *data, uint32_t size) {
#ifdef VOXEL_BUFFER_USE_MEMORY_POOL
	VoxelMemoryPool::get_singleton()->recycle(data, size);
#else
	memfree(data);
#endif
}

uint32_t g_depth_bit_counts[] = {
	8, 16, 32, 64
};
uint64_t g_depth_max_values[] = {
	0xff, // 8
	0xffff, // 16
	0xffffffff, // 32
	0xffffffffffffffff // 64
};

inline uint32_t get_depth_bit_count(VoxelBuffer::Depth d) {
	CRASH_COND(d < 0 || d >= VoxelBuffer::DEPTH_COUNT);
	return g_depth_bit_counts[d];
}

inline uint64_t get_max_value_for_depth(VoxelBuffer::Depth d) {
	CRASH_COND(d < 0 || d >= VoxelBuffer::DEPTH_COUNT);
	return g_depth_max_values[d];
}

inline uint64_t clamp_value_for_depth(uint64_t value, VoxelBuffer::Depth d) {
	const uint64_t max_val = get_max_value_for_depth(d);
	if (value >= max_val) {
		return max_val;
	}
	return value;
}

static_assert(sizeof(uint32_t) == sizeof(float), "uint32_t and float cannot be marshalled back and forth");
static_assert(sizeof(uint64_t) == sizeof(double), "uint64_t and double cannot be marshalled back and forth");

inline uint64_t real_to_raw_voxel(real_t value, VoxelBuffer::Depth depth) {
	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			return VoxelBuffer::norm_to_u8(value);

		case VoxelBuffer::DEPTH_16_BIT:
			return VoxelBuffer::norm_to_u16(value);

		case VoxelBuffer::DEPTH_32_BIT: {
			MarshallFloat m;
			m.f = value;
			return m.i;
		}
		case VoxelBuffer::DEPTH_64_BIT: {
			MarshallDouble m;
			m.d = value;
			return m.l;
		}
		default:
			CRASH_NOW();
			return 0;
	}
}

inline real_t raw_voxel_to_real(uint64_t value, VoxelBuffer::Depth depth) {
	// Depths below 32 are normalized between -1 and 1
	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			return VoxelBuffer::u8_to_norm(value);

		case VoxelBuffer::DEPTH_16_BIT:
			return VoxelBuffer::u16_to_norm(value);

		case VoxelBuffer::DEPTH_32_BIT: {
			MarshallFloat m;
			m.i = value;
			return m.f;
		}

		case VoxelBuffer::DEPTH_64_BIT: {
			MarshallDouble m;
			m.l = value;
			return m.d;
		}

		default:
			CRASH_NOW();
			return 0;
	}
}

} // namespace

const char *VoxelBuffer::CHANNEL_ID_HINT_STRING = "Type,Sdf,Data2,Data3,Data4,Data5,Data6,Data7";

VoxelBuffer::VoxelBuffer() {
	// Minecraft uses way more than 255 block types and there is room for eventual metadata such as rotation
	_channels[CHANNEL_TYPE].depth = VoxelBuffer::DEFAULT_TYPE_CHANNEL_DEPTH;
	_channels[CHANNEL_TYPE].defval = 0;

	// 16-bit is better on average to handle large worlds
	_channels[CHANNEL_SDF].depth = VoxelBuffer::DEFAULT_SDF_CHANNEL_DEPTH;
	_channels[CHANNEL_SDF].defval = 0xffff;
}

VoxelBuffer::~VoxelBuffer() {
	clear();
}

void VoxelBuffer::create(unsigned int sx, unsigned int sy, unsigned int sz) {
	ERR_FAIL_COND(sx > MAX_SIZE || sy > MAX_SIZE || sz > MAX_SIZE);

	clear_voxel_metadata();

	Vector3i new_size(sx, sy, sz);
	if (new_size != _size) {
		for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
			Channel &channel = _channels[i];
			if (channel.data) {
				// Channel already contained data
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
	_size = Vector3i();
	clear_voxel_metadata();
}

void VoxelBuffer::clear_channel(unsigned int channel_index, uint64_t clear_value) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	Channel &channel = _channels[channel_index];
	if (channel.data != nullptr) {
		delete_channel(channel_index);
	}
	channel.defval = clamp_value_for_depth(clear_value, channel.depth);
}

void VoxelBuffer::clear_channel_f(unsigned int channel_index, real_t clear_value) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	clear_channel(channel_index, real_to_raw_voxel(clear_value, channel.depth));
}

void VoxelBuffer::set_default_values(FixedArray<uint64_t, VoxelBuffer::MAX_CHANNELS> values) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		_channels[i].defval = clamp_value_for_depth(values[i], _channels[i].depth);
	}
}

uint64_t VoxelBuffer::get_voxel(int x, int y, int z, unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, 0);
	ERR_FAIL_COND_V_MSG(!is_position_valid(x, y, z), 0, String("At position ({0}, {1}, {2})").format(varray(x, y, z)));

	const Channel &channel = _channels[channel_index];

	if (channel.data != nullptr) {
		const uint32_t i = get_index(x, y, z);

		switch (channel.depth) {
			case DEPTH_8_BIT:
				return channel.data[i];

			case DEPTH_16_BIT:
				return reinterpret_cast<uint16_t *>(channel.data)[i];

			case DEPTH_32_BIT:
				return reinterpret_cast<uint32_t *>(channel.data)[i];

			case DEPTH_64_BIT:
				return reinterpret_cast<uint64_t *>(channel.data)[i];

			default:
				CRASH_NOW();
				return 0;
		}

	} else {
		return channel.defval;
	}
}

void VoxelBuffer::set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND_MSG(!is_position_valid(x, y, z), String("At position ({0}, {1}, {2})").format(varray(x, y, z)));

	Channel &channel = _channels[channel_index];

	value = clamp_value_for_depth(value, channel.depth);
	bool do_set = true;

	if (channel.data == nullptr) {
		if (channel.defval != value) {
			// Allocate channel with same initial values as defval
			create_channel(channel_index, _size, channel.defval);
		} else {
			do_set = false;
		}
	}

	if (do_set) {
		const uint32_t i = get_index(x, y, z);

		switch (channel.depth) {
			case DEPTH_8_BIT:
				channel.data[i] = value;
				break;

			case DEPTH_16_BIT:
				reinterpret_cast<uint16_t *>(channel.data)[i] = value;
				break;

			case DEPTH_32_BIT:
				reinterpret_cast<uint32_t *>(channel.data)[i] = value;
				break;

			case DEPTH_64_BIT:
				reinterpret_cast<uint64_t *>(channel.data)[i] = value;
				break;

			default:
				CRASH_NOW();
				break;
		}
	}
}

real_t VoxelBuffer::get_voxel_f(int x, int y, int z, unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, 0);
	return raw_voxel_to_real(get_voxel(x, y, z, channel_index), _channels[channel_index].depth);
}

void VoxelBuffer::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	set_voxel(real_to_raw_voxel(value, _channels[channel_index].depth), x, y, z, channel_index);
}

void VoxelBuffer::fill(uint64_t defval, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Channel &channel = _channels[channel_index];

	defval = clamp_value_for_depth(defval, channel.depth);

	if (channel.data == nullptr) {
		// Channel is already optimized and uniform
		if (channel.defval == defval) {
			// No change
			return;
		} else {
			// Just change default value
			channel.defval = defval;
			return;
		}
	}

	const unsigned int volume = get_volume();

	switch (channel.depth) {
		case DEPTH_8_BIT:
			memset(channel.data, defval, channel.size_in_bytes);
			break;

		case DEPTH_16_BIT:
			for (uint32_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint16_t *>(channel.data)[i] = defval;
			}
			break;

		case DEPTH_32_BIT:
			for (uint32_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint32_t *>(channel.data)[i] = defval;
			}
			break;

		case DEPTH_64_BIT:
			for (uint32_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint64_t *>(channel.data)[i] = defval;
			}
			break;

		default:
			CRASH_NOW();
			break;
	}
}

void VoxelBuffer::fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Vector3i::sort_min_max(min, max);

	min.clamp_to(Vector3i(0, 0, 0), _size + Vector3i(1, 1, 1));
	max.clamp_to(Vector3i(0, 0, 0), _size + Vector3i(1, 1, 1));
	const Vector3i area_size = max - min;

	if (area_size.x == 0 || area_size.y == 0 || area_size.z == 0) {
		return;
	}

	Channel &channel = _channels[channel_index];
	defval = clamp_value_for_depth(defval, channel.depth);

	if (channel.data == nullptr) {
		if (channel.defval == defval) {
			return;
		} else {
			create_channel(channel_index, _size, channel.defval);
		}
	}

	Vector3i pos;
	const unsigned int volume = get_volume();
	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
		for (pos.x = min.x; pos.x < max.x; ++pos.x) {
			const unsigned int dst_ri = get_index(pos.x, pos.y + min.y, pos.z);
			CRASH_COND(dst_ri >= volume);

			switch (channel.depth) {
				case DEPTH_8_BIT:
					// Fill row by row
					memset(&channel.data[dst_ri], defval, area_size.y * sizeof(uint8_t));
					break;

				case DEPTH_16_BIT:
					for (int i = 0; i < area_size.y; ++i) {
						((uint16_t *)channel.data)[dst_ri + i] = defval;
					}
					break;

				case DEPTH_32_BIT:
					for (int i = 0; i < area_size.y; ++i) {
						((uint32_t *)channel.data)[dst_ri + i] = defval;
					}
					break;

				case DEPTH_64_BIT:
					for (int i = 0; i < area_size.y; ++i) {
						((uint64_t *)channel.data)[dst_ri + i] = defval;
					}
					break;

				default:
					CRASH_NOW();
					break;
			}
		}
	}
}

void VoxelBuffer::fill_area_f(float fvalue, Vector3i min, Vector3i max, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	fill_area(real_to_raw_voxel(fvalue, channel.depth), min, max, channel_index);
}

void VoxelBuffer::fill_f(real_t value, unsigned int channel) {
	ERR_FAIL_INDEX(channel, MAX_CHANNELS);
	fill(real_to_raw_voxel(value, _channels[channel].depth), channel);
}

template <typename T>
inline bool is_uniform(const uint8_t *p_data, uint32_t size) {
	const T *data = (const T *)p_data;
	const T v0 = data[0];
	for (unsigned int i = 1; i < size; ++i) {
		if (data[i] != v0) {
			return false;
		}
	}
	return true;
}

bool VoxelBuffer::is_uniform(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);

	const Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		// Channel has been optimized
		return true;
	}

	const unsigned int volume = get_volume();

	// Channel isn't optimized, so must look at each voxel
	switch (channel.depth) {
		case DEPTH_8_BIT:
			return ::is_uniform<uint8_t>(channel.data, volume);
		case DEPTH_16_BIT:
			return ::is_uniform<uint16_t>(channel.data, volume);
		case DEPTH_32_BIT:
			return ::is_uniform<uint32_t>(channel.data, volume);
		case DEPTH_64_BIT:
			return ::is_uniform<uint64_t>(channel.data, volume);
		default:
			CRASH_NOW();
			break;
	}

	return true;
}

void VoxelBuffer::compress_uniform_channels() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		if (_channels[i].data != nullptr && is_uniform(i)) {
			// TODO More direct way
			const uint64_t v = get_voxel(0, 0, 0, i);
			clear_channel(i, v);
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

void VoxelBuffer::copy_format(const VoxelBuffer &other) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		set_channel_depth(i, other.get_channel_depth(i));
	}
}

void VoxelBuffer::copy_from(const VoxelBuffer &other) {
	// Copy all channels, assuming sizes and formats match
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		copy_from(other, i);
	}
}

void VoxelBuffer::copy_from(const VoxelBuffer &other, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND(other._size != _size);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ERR_FAIL_COND(other_channel.depth != channel.depth);

	if (other_channel.data != nullptr) {
		if (channel.data == nullptr) {
			create_channel_noinit(channel_index, _size);
		}
		CRASH_COND(channel.size_in_bytes != other_channel.size_in_bytes);
		memcpy(channel.data, other_channel.data, channel.size_in_bytes);

	} else if (channel.data != nullptr) {
		delete_channel(channel_index);
	}

	channel.defval = other_channel.defval;
	channel.depth = other_channel.depth;
}

inline void clip_copy_region_coord(int &src_min, int &src_max, const int src_size, int &dst_min, const int dst_size) {
	// Clamp source and shrink destination for moved borders
	if (src_min < 0) {
		dst_min += -src_min;
		src_min = 0;
	}
	if (src_max > src_size) {
		src_max = src_size;
	}
	// Clamp destination and shrink source for moved borders
	if (dst_min < 0) {
		src_min += -dst_min;
		dst_min = 0;
	}
	const int dst_w = src_max - src_min;
	const int dst_max = dst_min + dst_w;
	if (dst_max > dst_size) {
		src_max -= dst_max - dst_size;
	}
	// It is possible the source has negative size at this point, which means there is nothing to copy.
	// This must be checked by the caller.
}

inline void clip_copy_region(
		Vector3i &src_min, Vector3i &src_max, const Vector3i &src_size, Vector3i &dst_min, const Vector3i &dst_size) {
	clip_copy_region_coord(src_min.x, src_max.x, src_size.x, dst_min.x, dst_size.x);
	clip_copy_region_coord(src_min.y, src_max.y, src_size.y, dst_min.y, dst_size.y);
	clip_copy_region_coord(src_min.z, src_max.z, src_size.z, dst_min.z, dst_size.z);
}

void VoxelBuffer::copy_from(const VoxelBuffer &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min,
		unsigned int channel_index) {

	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ERR_FAIL_COND(other_channel.depth != channel.depth);

	if (channel.data == nullptr && other_channel.data == nullptr && channel.defval == other_channel.defval) {
		// No action needed
		return;
	}

	Vector3i::sort_min_max(src_min, src_max);

	clip_copy_region(src_min, src_max, other._size, dst_min, _size);

	const Vector3i area_size = src_max - src_min;

	if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
		// Degenerate area, we'll not copy anything.
		return;
	}

	if (area_size == _size && area_size == other._size) {
		// Equivalent of full copy between two blocks of same size
		copy_from(other, channel_index);

	} else {
		if (other_channel.data != nullptr) {

			if (channel.data == nullptr) {
				create_channel(channel_index, _size, channel.defval);
			}

			if (channel.depth == DEPTH_8_BIT) {
				// Native format
				// Copy row by row
				Vector3i pos;
				for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
					for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
						// Row direction is Y
						const unsigned int src_ri =
								other.get_index(pos.x + src_min.x, pos.y + src_min.y, pos.z + src_min.z);
						const unsigned int dst_ri = get_index(pos.x + dst_min.x, pos.y + dst_min.y, pos.z + dst_min.z);
						memcpy(&channel.data[dst_ri], &other_channel.data[src_ri], area_size.y * sizeof(uint8_t));
					}
				}

			} else if (channel.depth == DEPTH_16_BIT) {
				Vector3i pos;
				for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
					for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
						const unsigned int src_ri =
								2 * other.get_index(pos.x + src_min.x, pos.y + src_min.y, pos.z + src_min.z);
						const unsigned int dst_ri =
								2 * get_index(pos.x + dst_min.x, pos.y + dst_min.y, pos.z + dst_min.z);
						memcpy(&channel.data[dst_ri], &other_channel.data[src_ri], area_size.y * sizeof(uint16_t));
					}
				}

			} else {
				VOXEL_PROFILE_SCOPE();
				// TODO Optimized versions
				Vector3i pos;
				for (pos.z = 0; pos.z < area_size.z; ++pos.z) {
					for (pos.x = 0; pos.x < area_size.x; ++pos.x) {
						for (pos.y = 0; pos.y < area_size.y; ++pos.y) {
							const uint64_t v = other.get_voxel(src_min + pos, channel_index);
							set_voxel(v, dst_min + pos, channel_index);
						}
					}
				}
			}

		} else if (channel.defval != other_channel.defval) {
			if (channel.data == nullptr) {
				create_channel(channel_index, _size, channel.defval);
			}
			fill_area(other_channel.defval, dst_min, dst_min + area_size, channel_index);
		}
	}
}

Ref<VoxelBuffer> VoxelBuffer::duplicate(bool include_metadata) const {
	VoxelBuffer *d = memnew(VoxelBuffer);
	d->create(_size);
	for (unsigned int i = 0; i < _channels.size(); ++i) {
		d->set_channel_depth(i, _channels[i].depth);
	}
	d->copy_from(*this);
	if (include_metadata) {
		d->copy_voxel_metadata(*this);
	}
	return Ref<VoxelBuffer>(d);
}

bool VoxelBuffer::get_channel_raw(unsigned int channel_index, ArraySlice<uint8_t> &slice) const {
	const Channel &channel = _channels[channel_index];
	if (channel.data != nullptr) {
		slice = ArraySlice<uint8_t>(channel.data, 0, channel.size_in_bytes);
		return true;
	}
	slice = ArraySlice<uint8_t>();
	return false;
}

void VoxelBuffer::create_channel(int i, Vector3i size, uint64_t defval) {
	create_channel_noinit(i, size);
	fill(defval, i);
}

uint32_t VoxelBuffer::get_size_in_bytes_for_volume(Vector3i size, Depth depth) {
	// Calculate appropriate size based on bit depth
	const unsigned int volume = size.x * size.y * size.z;
	const unsigned int bits = volume * ::get_depth_bit_count(depth);
	const unsigned int size_in_bytes = (bits >> 3);
	return size_in_bytes;
}

void VoxelBuffer::create_channel_noinit(int i, Vector3i size) {
	Channel &channel = _channels[i];
	uint32_t size_in_bytes = get_size_in_bytes_for_volume(size, channel.depth);
	CRASH_COND(channel.data != nullptr);
	channel.data = allocate_channel_data(size_in_bytes);
	channel.size_in_bytes = size_in_bytes;
}

void VoxelBuffer::delete_channel(int i) {
	Channel &channel = _channels[i];
	ERR_FAIL_COND(channel.data == nullptr);
	free_channel_data(channel.data, channel.size_in_bytes);
	channel.data = nullptr;
	channel.size_in_bytes = 0;
}

void VoxelBuffer::downscale_to(VoxelBuffer &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const {
	// TODO Align input to multiple of two

	src_min.clamp_to(Vector3i(), _size);
	src_max.clamp_to(Vector3i(), _size + Vector3i(1));

	Vector3i dst_max = dst_min + ((src_max - src_min) >> 1);

	// TODO This will be wrong if it overlaps the border?
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
					const Vector3i src_pos = src_min + ((pos - dst_min) << 1);

					// TODO Remove check once it works
					CRASH_COND(!is_position_valid(src_pos.x, src_pos.y, src_pos.z));

					uint64_t v;
					if (src_channel.data) {
						// TODO Optimized version?
						v = get_voxel(src_pos, channel_index);
					} else {
						v = src_channel.defval;
					}

					dst.set_voxel(v, pos, channel_index);
				}
			}
		}
	}
}

Ref<VoxelTool> VoxelBuffer::get_voxel_tool() {
	// I can't make this function `const`, because `Ref<T>` has no constructor taking a `const T*`.
	// The compiler would then choose Ref<T>(const Variant&), which fumbles `this` into a null pointer
	Ref<VoxelBuffer> vb(this);
	return Ref<VoxelTool>(memnew(VoxelToolBuffer(vb)));
}

bool VoxelBuffer::equals(const VoxelBuffer &p_other) const {
	if (p_other._size != _size) {
		return false;
	}

	for (int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {
		const Channel &channel = _channels[channel_index];
		const Channel &other_channel = p_other._channels[channel_index];

		if ((channel.data == nullptr) != (other_channel.data == nullptr)) {
			// Note: they could still logically be equal if one channel contains uniform voxel memory
			return false;
		}

		if (channel.depth != other_channel.depth) {
			return false;
		}

		if (channel.data == nullptr) {
			if (channel.defval != other_channel.defval) {
				return false;
			}

		} else {
			ERR_FAIL_COND_V(channel.size_in_bytes != other_channel.size_in_bytes, false);
			for (unsigned int i = 0; i < channel.size_in_bytes; ++i) {
				if (channel.data[i] != other_channel.data[i]) {
					return false;
				}
			}
		}
	}

	return true;
}

void VoxelBuffer::set_channel_depth(unsigned int channel_index, Depth new_depth) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_INDEX(new_depth, DEPTH_COUNT);
	Channel &channel = _channels[channel_index];
	if (channel.depth == new_depth) {
		return;
	}
	if (channel.data != nullptr) {
		// TODO Implement conversion and do it when specified
		WARN_PRINT("Changing VoxelBuffer depth with present data, this will reset the channel");
		delete_channel(channel_index);
	}
	channel.defval = clamp_value_for_depth(channel.defval, new_depth);
	channel.depth = new_depth;
}

VoxelBuffer::Depth VoxelBuffer::get_channel_depth(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, DEPTH_8_BIT);
	return _channels[channel_index].depth;
}

uint32_t VoxelBuffer::get_depth_bit_count(Depth d) {
	return ::get_depth_bit_count(d);
}

float VoxelBuffer::get_sdf_quantization_scale(Depth d) {
	switch (d) {
		// Normalized
		case DEPTH_8_BIT:
			return VoxelConstants::QUANTIZED_SDF_8_BITS_SCALE;
		case DEPTH_16_BIT:
			return VoxelConstants::QUANTIZED_SDF_16_BITS_SCALE;
		// Direct
		default:
			return 1.f;
	}
}

void VoxelBuffer::set_block_metadata(Variant meta) {
	_block_metadata = meta;
}

Variant VoxelBuffer::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(!is_position_valid(pos), Variant());
	const Map<Vector3i, Variant>::Element *elem = _voxel_metadata.find(pos);
	if (elem != nullptr) {
		return elem->value();
	} else {
		return Variant();
	}
}

void VoxelBuffer::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(!is_position_valid(pos));
	if (meta.get_type() == Variant::NIL) {
		_voxel_metadata.erase(pos);
	} else {
		_voxel_metadata[pos] = meta;
	}
}

void VoxelBuffer::for_each_voxel_metadata(Ref<FuncRef> callback) const {
	ERR_FAIL_COND(callback.is_null());
	const Map<Vector3i, Variant>::Element *elem = _voxel_metadata.front();

	while (elem != nullptr) {
		const Variant key = elem->key().to_vec3();
		const Variant *args[2] = { &key, &elem->value() };
		Variant::CallError err;
		callback->call_func(args, 2, err);

		ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK,
				String("FuncRef call failed at {0}").format(varray(key)));
		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));

		elem = elem->next();
	}
}

void VoxelBuffer::for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Rect3i box) const {
	ERR_FAIL_COND(callback.is_null());
	const Map<Vector3i, Variant>::Element *elem = _voxel_metadata.front();

	while (elem != nullptr) {
		if (box.contains(elem->key())) {
			const Variant key = elem->key().to_vec3();
			const Variant *args[2] = { &key, &elem->value() };
			Variant::CallError err;
			callback->call_func(args, 2, err);

			ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK,
					String("FuncRef call failed at {0}").format(varray(key)));
			// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
			// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
			// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
		}
		elem = elem->next();
	}
}

void VoxelBuffer::clear_voxel_metadata() {
	_voxel_metadata.clear();
}

void VoxelBuffer::clear_voxel_metadata_in_area(Rect3i box) {
	Map<Vector3i, Variant>::Element *elem = _voxel_metadata.front();
	while (elem != nullptr) {
		Map<Vector3i, Variant>::Element *next_elem = elem->next();
		if (box.contains(elem->key())) {
			_voxel_metadata.erase(elem);
		}
		elem = next_elem;
	}
}

void VoxelBuffer::copy_voxel_metadata_in_area(Ref<VoxelBuffer> src_buffer, Rect3i src_box, Vector3i dst_origin) {
	ERR_FAIL_COND(src_buffer.is_null());
	ERR_FAIL_COND(src_buffer->is_box_valid(src_box));

	const Rect3i clipped_src_box = src_box.clipped(Rect3i(src_box.pos - dst_origin, _size));
	const Vector3i clipped_dst_offset = dst_origin + clipped_src_box.pos - src_box.pos;

	const Map<Vector3i, Variant>::Element *elem = src_buffer->_voxel_metadata.front();

	while (elem != nullptr) {
		const Vector3i src_pos = elem->key();
		if (src_box.contains(src_pos)) {
			const Vector3i dst_pos = src_pos + clipped_dst_offset;
			CRASH_COND(!is_position_valid(dst_pos));
			_voxel_metadata[dst_pos] = elem->value().duplicate();
		}
		elem = elem->next();
	}
}

void VoxelBuffer::copy_voxel_metadata(const VoxelBuffer &src_buffer) {
	ERR_FAIL_COND(src_buffer.get_size() != _size);

	const Map<Vector3i, Variant>::Element *elem = src_buffer._voxel_metadata.front();

	while (elem != nullptr) {
		const Vector3i pos = elem->key();
		_voxel_metadata[pos] = elem->value().duplicate();
		elem = elem->next();
	}

	_block_metadata = src_buffer._block_metadata.duplicate();
}

Ref<Image> VoxelBuffer::debug_print_sdf_to_image_top_down() {
	Image *im = memnew(Image);
	im->create(_size.x, _size.z, false, Image::FORMAT_RGB8);
	im->lock();
	Vector3i pos;
	for (pos.z = 0; pos.z < _size.z; ++pos.z) {
		for (pos.x = 0; pos.x < _size.x; ++pos.x) {
			for (pos.y = _size.y - 1; pos.y >= 0; --pos.y) {
				float v = get_voxel_f(pos.x, pos.y, pos.z, CHANNEL_SDF);
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

void VoxelBuffer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create", "sx", "sy", "sz"), &VoxelBuffer::_b_create);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBuffer::clear);

	ClassDB::bind_method(D_METHOD("get_size"), &VoxelBuffer::_b_get_size);
	ClassDB::bind_method(D_METHOD("get_size_x"), &VoxelBuffer::get_size_x);
	ClassDB::bind_method(D_METHOD("get_size_y"), &VoxelBuffer::get_size_y);
	ClassDB::bind_method(D_METHOD("get_size_z"), &VoxelBuffer::get_size_z);

	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "channel"),
			&VoxelBuffer::_b_set_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_f", "value", "x", "y", "z", "channel"),
			&VoxelBuffer::_b_set_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel_v", "value", "pos", "channel"), &VoxelBuffer::_b_set_voxel_v, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "channel"), &VoxelBuffer::_b_get_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_f", "x", "y", "z", "channel"), &VoxelBuffer::get_voxel_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_voxel_tool"), &VoxelBuffer::get_voxel_tool);

	ClassDB::bind_method(D_METHOD("get_channel_depth", "channel"), &VoxelBuffer::get_channel_depth);
	ClassDB::bind_method(D_METHOD("set_channel_depth", "channel", "depth"), &VoxelBuffer::set_channel_depth);

	ClassDB::bind_method(D_METHOD("fill", "value", "channel"), &VoxelBuffer::fill, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_f", "value", "channel"), &VoxelBuffer::fill_f, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("fill_area", "value", "min", "max", "channel"),
			&VoxelBuffer::_b_fill_area, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("copy_channel_from", "other", "channel"), &VoxelBuffer::_b_copy_channel_from);
	ClassDB::bind_method(D_METHOD("copy_channel_from_area", "other", "src_min", "src_max", "dst_min", "channel"),
			&VoxelBuffer::_b_copy_channel_from_area);
	ClassDB::bind_method(D_METHOD("downscale_to", "dst", "src_min", "src_max", "dst_min"),
			&VoxelBuffer::_b_downscale_to);

	ClassDB::bind_method(D_METHOD("is_uniform", "channel"), &VoxelBuffer::is_uniform);
	// TODO Rename `compress_uniform_channels`
	ClassDB::bind_method(D_METHOD("optimize"), &VoxelBuffer::compress_uniform_channels);
	ClassDB::bind_method(D_METHOD("get_channel_compression", "channel"), &VoxelBuffer::get_channel_compression);

	ClassDB::bind_method(D_METHOD("get_block_metadata"), &VoxelBuffer::get_block_metadata);
	ClassDB::bind_method(D_METHOD("set_block_metadata", "meta"), &VoxelBuffer::set_block_metadata);
	ClassDB::bind_method(D_METHOD("get_voxel_metadata", "pos"), &VoxelBuffer::_b_get_voxel_metadata);
	ClassDB::bind_method(D_METHOD("set_voxel_metadata", "pos", "value"), &VoxelBuffer::_b_set_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata", "callback"), &VoxelBuffer::for_each_voxel_metadata);
	ClassDB::bind_method(D_METHOD("for_each_voxel_metadata_in_area", "callback", "min_pos", "max_pos"),
			&VoxelBuffer::_b_for_each_voxel_metadata_in_area);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata"), &VoxelBuffer::clear_voxel_metadata);
	ClassDB::bind_method(D_METHOD("clear_voxel_metadata_in_area", "min_pos", "max_pos"),
			&VoxelBuffer::_b_clear_voxel_metadata_in_area);
	ClassDB::bind_method(
			D_METHOD("copy_voxel_metadata_in_area", "src_buffer", "src_min_pos", "src_max_pos", "dst_min_pos"),
			&VoxelBuffer::_b_copy_voxel_metadata_in_area);

	BIND_ENUM_CONSTANT(CHANNEL_TYPE);
	BIND_ENUM_CONSTANT(CHANNEL_SDF);
	BIND_ENUM_CONSTANT(CHANNEL_COLOR);
	BIND_ENUM_CONSTANT(CHANNEL_DATA3);
	BIND_ENUM_CONSTANT(CHANNEL_DATA4);
	BIND_ENUM_CONSTANT(CHANNEL_DATA5);
	BIND_ENUM_CONSTANT(CHANNEL_DATA6);
	BIND_ENUM_CONSTANT(CHANNEL_DATA7);
	BIND_ENUM_CONSTANT(MAX_CHANNELS);

	BIND_ENUM_CONSTANT(DEPTH_8_BIT);
	BIND_ENUM_CONSTANT(DEPTH_16_BIT);
	BIND_ENUM_CONSTANT(DEPTH_32_BIT);
	BIND_ENUM_CONSTANT(DEPTH_64_BIT);
	BIND_ENUM_CONSTANT(DEPTH_COUNT);

	BIND_ENUM_CONSTANT(COMPRESSION_NONE);
	BIND_ENUM_CONSTANT(COMPRESSION_UNIFORM);
	BIND_ENUM_CONSTANT(COMPRESSION_COUNT);

	BIND_CONSTANT(MAX_SIZE);
}

void VoxelBuffer::_b_copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	copy_from(**other, channel);
}

void VoxelBuffer::_b_copy_channel_from_area(Ref<VoxelBuffer> other, Vector3 src_min, Vector3 src_max, Vector3 dst_min,
		unsigned int channel) {
	ERR_FAIL_COND(other.is_null());
	copy_from(**other, Vector3i(src_min), Vector3i(src_max), Vector3i(dst_min), channel);
}

void VoxelBuffer::_b_downscale_to(Ref<VoxelBuffer> dst, Vector3 src_min, Vector3 src_max, Vector3 dst_min) const {
	ERR_FAIL_COND(dst.is_null());
	downscale_to(**dst, Vector3i(src_min), Vector3i(src_max), Vector3i(dst_min));
}

void VoxelBuffer::_b_for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Vector3 min_pos, Vector3 max_pos) {
	for_each_voxel_metadata_in_area(callback, Rect3i::from_min_max(Vector3i(min_pos), Vector3i(max_pos)));
}

void VoxelBuffer::_b_clear_voxel_metadata_in_area(Vector3 min_pos, Vector3 max_pos) {
	clear_voxel_metadata_in_area(Rect3i::from_min_max(Vector3i(min_pos), Vector3i(max_pos)));
}

void VoxelBuffer::_b_copy_voxel_metadata_in_area(Ref<VoxelBuffer> src_buffer, Vector3 src_min_pos, Vector3 src_max_pos,
		Vector3 dst_pos) {
	copy_voxel_metadata_in_area(
			src_buffer, Rect3i::from_min_max(Vector3i(src_min_pos), Vector3i(src_max_pos)), dst_pos);
}
