#define VOXEL_BUFFER_USE_MEMORY_POOL

#ifdef VOXEL_BUFFER_USE_MEMORY_POOL
#include "voxel_memory_pool.h"
#endif

#include "../util/funcs.h"
#include "../util/profiling.h"
#include "voxel_buffer_internal.h"

#include <core/func_ref.h>
#include <core/image.h>
#include <core/io/marshalls.h>
#include <core/math/math_funcs.h>
#include <string.h>

namespace {

inline uint8_t *allocate_channel_data(size_t size) {
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

uint64_t g_depth_max_values[] = {
	0xff, // 8
	0xffff, // 16
	0xffffffff, // 32
	0xffffffffffffffff // 64
};

inline uint32_t get_depth_bit_count(VoxelBufferInternal::Depth d) {
	CRASH_COND(d < 0 || d >= VoxelBufferInternal::DEPTH_COUNT);
	return VoxelBufferInternal::get_depth_byte_count(d) << 3;
}

inline uint64_t get_max_value_for_depth(VoxelBufferInternal::Depth d) {
	CRASH_COND(d < 0 || d >= VoxelBufferInternal::DEPTH_COUNT);
	return g_depth_max_values[d];
}

inline uint64_t clamp_value_for_depth(uint64_t value, VoxelBufferInternal::Depth d) {
	const uint64_t max_val = get_max_value_for_depth(d);
	if (value >= max_val) {
		return max_val;
	}
	return value;
}

static_assert(sizeof(uint32_t) == sizeof(float), "uint32_t and float cannot be marshalled back and forth");
static_assert(sizeof(uint64_t) == sizeof(double), "uint64_t and double cannot be marshalled back and forth");

inline uint64_t real_to_raw_voxel(real_t value, VoxelBufferInternal::Depth depth) {
	switch (depth) {
		case VoxelBufferInternal::DEPTH_8_BIT:
			return norm_to_u8(value);

		case VoxelBufferInternal::DEPTH_16_BIT:
			return norm_to_u16(value);

		case VoxelBufferInternal::DEPTH_32_BIT: {
			MarshallFloat m;
			m.f = value;
			return m.i;
		}
		case VoxelBufferInternal::DEPTH_64_BIT: {
			MarshallDouble m;
			m.d = value;
			return m.l;
		}
		default:
			CRASH_NOW();
			return 0;
	}
}

inline real_t raw_voxel_to_real(uint64_t value, VoxelBufferInternal::Depth depth) {
	// Depths below 32 are normalized between -1 and 1
	switch (depth) {
		case VoxelBufferInternal::DEPTH_8_BIT:
			return u8_to_norm(value);

		case VoxelBufferInternal::DEPTH_16_BIT:
			return u16_to_norm(value);

		case VoxelBufferInternal::DEPTH_32_BIT: {
			MarshallFloat m;
			m.i = value;
			return m.f;
		}

		case VoxelBufferInternal::DEPTH_64_BIT: {
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

VoxelBufferInternal::VoxelBufferInternal() {
	// Minecraft uses way more than 255 block types and there is room for eventual metadata such as rotation
	_channels[CHANNEL_TYPE].depth = VoxelBufferInternal::DEFAULT_TYPE_CHANNEL_DEPTH;
	_channels[CHANNEL_TYPE].defval = 0;

	// 16-bit is better on average to handle large worlds
	_channels[CHANNEL_SDF].depth = VoxelBufferInternal::DEFAULT_SDF_CHANNEL_DEPTH;
	_channels[CHANNEL_SDF].defval = 0xffff;

	_channels[CHANNEL_INDICES].depth = VoxelBufferInternal::DEPTH_16_BIT;
	_channels[CHANNEL_INDICES].defval = encode_indices_to_packed_u16(0, 1, 2, 3);

	_channels[CHANNEL_WEIGHTS].depth = VoxelBufferInternal::DEPTH_16_BIT;
	_channels[CHANNEL_WEIGHTS].defval = encode_weights_to_packed_u16(15, 0, 0, 0);
}

VoxelBufferInternal::VoxelBufferInternal(VoxelBufferInternal &&src) {
	src.move_to(*this);
}

VoxelBufferInternal::~VoxelBufferInternal() {
	clear();
}

VoxelBufferInternal &VoxelBufferInternal::operator=(VoxelBufferInternal &&src) {
	src.move_to(*this);
	return *this;
}

void VoxelBufferInternal::create(unsigned int sx, unsigned int sy, unsigned int sz) {
	ERR_FAIL_COND(sx > MAX_SIZE || sy > MAX_SIZE || sz > MAX_SIZE);
#ifdef TOOLS_ENABLED
	if (sx == 0 || sy == 0 || sz == 0) {
		WARN_PRINT(String("VoxelBuffer::create called with empty size ({0}, {1}, {2})").format(varray(sx, sy, sz)));
	}
#endif

	clear_voxel_metadata();

	const Vector3i new_size(sx, sy, sz);
	if (new_size != _size) {
		// Assign size first, because `create_channel` uses it
		_size = new_size;
		for (unsigned int i = 0; i < _channels.size(); ++i) {
			Channel &channel = _channels[i];
			if (channel.data != nullptr) {
				// Channel already contained data
				delete_channel(i);
				ERR_FAIL_COND(!create_channel(i, channel.defval));
			}
		}
	}
}

void VoxelBufferInternal::create(Vector3i size) {
	create(size.x, size.y, size.z);
}

void VoxelBufferInternal::clear() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		Channel &channel = _channels[i];
		if (channel.data != nullptr) {
			delete_channel(i);
		}
	}
	_size = Vector3i();
	clear_voxel_metadata();
}

void VoxelBufferInternal::clear_channel(unsigned int channel_index, uint64_t clear_value) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	Channel &channel = _channels[channel_index];
	if (channel.data != nullptr) {
		delete_channel(channel_index);
	}
	channel.defval = clamp_value_for_depth(clear_value, channel.depth);
}

void VoxelBufferInternal::clear_channel_f(unsigned int channel_index, real_t clear_value) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	clear_channel(channel_index, real_to_raw_voxel(clear_value, channel.depth));
}

void VoxelBufferInternal::set_default_values(FixedArray<uint64_t, VoxelBufferInternal::MAX_CHANNELS> values) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		_channels[i].defval = clamp_value_for_depth(values[i], _channels[i].depth);
	}
}

uint64_t VoxelBufferInternal::get_voxel(int x, int y, int z, unsigned int channel_index) const {
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

void VoxelBufferInternal::set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND_MSG(!is_position_valid(x, y, z), String("At position ({0}, {1}, {2})").format(varray(x, y, z)));

	Channel &channel = _channels[channel_index];

	value = clamp_value_for_depth(value, channel.depth);
	bool do_set = true;

	if (channel.data == nullptr) {
		if (channel.defval != value) {
			// Allocate channel with same initial values as defval
			ERR_FAIL_COND(!create_channel(channel_index, channel.defval));
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

real_t VoxelBufferInternal::get_voxel_f(int x, int y, int z, unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, 0);
	return raw_voxel_to_real(get_voxel(x, y, z, channel_index), _channels[channel_index].depth);
}

void VoxelBufferInternal::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	set_voxel(real_to_raw_voxel(value, _channels[channel_index].depth), x, y, z, channel_index);
}

void VoxelBufferInternal::fill(uint64_t defval, unsigned int channel_index) {
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

	const size_t volume = get_volume();
#ifdef DEBUG_ENABLED
	CRASH_COND(channel.size_in_bytes != get_size_in_bytes_for_volume(_size, channel.depth));
#endif

	switch (channel.depth) {
		case DEPTH_8_BIT:
			memset(channel.data, defval, channel.size_in_bytes);
			break;

		case DEPTH_16_BIT:
			for (size_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint16_t *>(channel.data)[i] = defval;
			}
			break;

		case DEPTH_32_BIT:
			for (size_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint32_t *>(channel.data)[i] = defval;
			}
			break;

		case DEPTH_64_BIT:
			for (size_t i = 0; i < volume; ++i) {
				reinterpret_cast<uint64_t *>(channel.data)[i] = defval;
			}
			break;

		default:
			CRASH_NOW();
			break;
	}
}

void VoxelBufferInternal::fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index) {
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
			ERR_FAIL_COND(!create_channel(channel_index, channel.defval));
		}
	}

	Vector3i pos;
	const size_t volume = get_volume();

	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
		for (pos.x = min.x; pos.x < max.x; ++pos.x) {
			const size_t dst_ri = get_index(pos.x, pos.y + min.y, pos.z);
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

void VoxelBufferInternal::fill_area_f(float fvalue, Vector3i min, Vector3i max, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	fill_area(real_to_raw_voxel(fvalue, channel.depth), min, max, channel_index);
}

void VoxelBufferInternal::fill_f(real_t value, unsigned int channel) {
	ERR_FAIL_INDEX(channel, MAX_CHANNELS);
	fill(real_to_raw_voxel(value, _channels[channel].depth), channel);
}

template <typename T>
inline bool is_uniform_b(const uint8_t *data, size_t item_count) {
	return is_uniform<T>(reinterpret_cast<const T *>(data), item_count);
}

bool VoxelBufferInternal::is_uniform(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, true);

	const Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		// Channel has been optimized
		return true;
	}

	const size_t volume = get_volume();

	// Channel isn't optimized, so must look at each voxel
	switch (channel.depth) {
		case DEPTH_8_BIT:
			return ::is_uniform_b<uint8_t>(channel.data, volume);
		case DEPTH_16_BIT:
			return ::is_uniform_b<uint16_t>(channel.data, volume);
		case DEPTH_32_BIT:
			return ::is_uniform_b<uint32_t>(channel.data, volume);
		case DEPTH_64_BIT:
			return ::is_uniform_b<uint64_t>(channel.data, volume);
		default:
			CRASH_NOW();
			break;
	}

	return true;
}

void VoxelBufferInternal::compress_uniform_channels() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		if (_channels[i].data != nullptr && is_uniform(i)) {
			// TODO More direct way
			const uint64_t v = get_voxel(0, 0, 0, i);
			clear_channel(i, v);
		}
	}
}

void VoxelBufferInternal::decompress_channel(unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		ERR_FAIL_COND(!create_channel(channel_index, channel.defval));
	}
}

VoxelBufferInternal::Compression VoxelBufferInternal::get_channel_compression(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, VoxelBufferInternal::COMPRESSION_NONE);
	const Channel &channel = _channels[channel_index];
	if (channel.data == nullptr) {
		return COMPRESSION_UNIFORM;
	}
	return COMPRESSION_NONE;
}

void VoxelBufferInternal::copy_format(const VoxelBufferInternal &other) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		set_channel_depth(i, other.get_channel_depth(i));
	}
}

void VoxelBufferInternal::copy_from(const VoxelBufferInternal &other) {
	// Copy all channels, assuming sizes and formats match
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		copy_from(other, i);
	}
}

void VoxelBufferInternal::copy_from(const VoxelBufferInternal &other, unsigned int channel_index) {
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	ERR_FAIL_COND(other._size != _size);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ERR_FAIL_COND(other_channel.depth != channel.depth);

	if (other_channel.data != nullptr) {
		if (channel.data == nullptr) {
			ERR_FAIL_COND(!create_channel_noinit(channel_index, _size));
		}
		CRASH_COND(channel.size_in_bytes != other_channel.size_in_bytes);
		memcpy(channel.data, other_channel.data, channel.size_in_bytes);

	} else if (channel.data != nullptr) {
		delete_channel(channel_index);
	}

	channel.defval = other_channel.defval;
	channel.depth = other_channel.depth;
}

// TODO Disallow copying from overlapping areas of the same buffer
void VoxelBufferInternal::copy_from(const VoxelBufferInternal &other, Vector3i src_min, Vector3i src_max,
		Vector3i dst_min, unsigned int channel_index) {
	//
	ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ERR_FAIL_COND(other_channel.depth != channel.depth);

	if (channel.data == nullptr && other_channel.data == nullptr && channel.defval == other_channel.defval) {
		// No action needed
		return;
	}

	if (other_channel.data != nullptr) {
		if (channel.data == nullptr) {
			// Note, we do this even if the pasted data happens to be all the same value as our current channel.
			// We assume that this case is not frequent enough to bother, and compression can happen later
			ERR_FAIL_COND(!create_channel(channel_index, channel.defval));
		}
		const unsigned int item_size = get_depth_byte_count(channel.depth);
		Span<const uint8_t> src(other_channel.data, other_channel.size_in_bytes);
		Span<uint8_t> dst(channel.data, channel.size_in_bytes);
		copy_3d_region_zxy(dst, _size, dst_min, src, other._size, src_min, src_max, item_size);

	} else if (channel.defval != other_channel.defval) {
		// This logic is still required due to how source and destination regions can be specified.
		// The actual size of the destination area must be determined from the source area, after it has been clipped.
		Vector3i::sort_min_max(src_min, src_max);
		clip_copy_region(src_min, src_max, other._size, dst_min, _size);
		const Vector3i area_size = src_max - src_min;
		if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
			// Degenerate area, we'll not copy anything.
			return;
		}
		fill_area(other_channel.defval, dst_min, dst_min + area_size, channel_index);
	}
}

void VoxelBufferInternal::duplicate_to(VoxelBufferInternal &dst, bool include_metadata) const {
	dst.create(_size);
	for (unsigned int i = 0; i < _channels.size(); ++i) {
		dst.set_channel_depth(i, _channels[i].depth);
	}
	dst.copy_from(*this);
	if (include_metadata) {
		dst.copy_voxel_metadata(*this);
	}
}

void VoxelBufferInternal::move_to(VoxelBufferInternal &dst) {
	if (this == &dst) {
		return;
	}

	dst.clear();

	dst._channels = _channels;
	dst._size = _size;

	// TODO Optimization: Godot needs move semantics
	dst._block_metadata = _block_metadata;
	_block_metadata = Variant();

	// TODO Optimization: Godot needs move semantics
	dst._voxel_metadata = _voxel_metadata;
	_voxel_metadata.clear();

	for (unsigned int i = 0; i < _channels.size(); ++i) {
		Channel &channel = _channels[i];
		channel.data = nullptr;
		channel.size_in_bytes = 0;
	}
}

bool VoxelBufferInternal::get_channel_raw(unsigned int channel_index, Span<uint8_t> &slice) const {
	const Channel &channel = _channels[channel_index];
	if (channel.data != nullptr) {
		slice = Span<uint8_t>(channel.data, 0, channel.size_in_bytes);
		return true;
	}
	slice = Span<uint8_t>();
	return false;
}

bool VoxelBufferInternal::create_channel(int i, uint64_t defval) {
	if (!create_channel_noinit(i, _size)) {
		return false;
	}
	fill(defval, i);
	return true;
}

uint32_t VoxelBufferInternal::get_size_in_bytes_for_volume(Vector3i size, Depth depth) {
	// Calculate appropriate size based on bit depth
	const unsigned int volume = size.x * size.y * size.z;
	const unsigned int bits = volume * ::get_depth_bit_count(depth);
	const unsigned int size_in_bytes = (bits >> 3);
	return size_in_bytes;
}

bool VoxelBufferInternal::create_channel_noinit(int i, Vector3i size) {
	Channel &channel = _channels[i];
	const size_t size_in_bytes = get_size_in_bytes_for_volume(size, channel.depth);
	CRASH_COND(channel.data != nullptr);
	channel.data = allocate_channel_data(size_in_bytes);
	ERR_FAIL_COND_V(channel.data == nullptr, false);
	channel.size_in_bytes = size_in_bytes;
	return true;
}

void VoxelBufferInternal::delete_channel(int i) {
	Channel &channel = _channels[i];
	ERR_FAIL_COND(channel.data == nullptr);
	// Don't use `_size` to obtain `data` byte count, since we could have changed `_size` up-front during a create().
	// `size_in_bytes` reflects what is currently allocated inside `data`, regardless of anything else.
	free_channel_data(channel.data, channel.size_in_bytes);
	channel.data = nullptr;
	channel.size_in_bytes = 0;
}

void VoxelBufferInternal::downscale_to(VoxelBufferInternal &dst, Vector3i src_min, Vector3i src_max,
		Vector3i dst_min) const {
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

bool VoxelBufferInternal::equals(const VoxelBufferInternal &p_other) const {
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
			for (size_t i = 0; i < channel.size_in_bytes; ++i) {
				if (channel.data[i] != other_channel.data[i]) {
					return false;
				}
			}
		}
	}

	return true;
}

void VoxelBufferInternal::set_channel_depth(unsigned int channel_index, Depth new_depth) {
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

VoxelBufferInternal::Depth VoxelBufferInternal::get_channel_depth(unsigned int channel_index) const {
	ERR_FAIL_INDEX_V(channel_index, MAX_CHANNELS, DEPTH_8_BIT);
	return _channels[channel_index].depth;
}

uint32_t VoxelBufferInternal::get_depth_bit_count(Depth d) {
	return ::get_depth_bit_count(d);
}

float VoxelBufferInternal::get_sdf_quantization_scale(Depth d) {
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

void VoxelBufferInternal::set_block_metadata(Variant meta) {
	_block_metadata = meta;
}

Variant VoxelBufferInternal::get_voxel_metadata(Vector3i pos) const {
	ERR_FAIL_COND_V(!is_position_valid(pos), Variant());
	const Map<Vector3i, Variant>::Element *elem = _voxel_metadata.find(pos);
	if (elem != nullptr) {
		return elem->value();
	} else {
		return Variant();
	}
}

void VoxelBufferInternal::set_voxel_metadata(Vector3i pos, Variant meta) {
	ERR_FAIL_COND(!is_position_valid(pos));
	if (meta.get_type() == Variant::NIL) {
		_voxel_metadata.erase(pos);
	} else {
		_voxel_metadata[pos] = meta;
	}
}

void VoxelBufferInternal::for_each_voxel_metadata(Ref<FuncRef> callback) const {
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

void VoxelBufferInternal::for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Box3i box) const {
	ERR_FAIL_COND(callback.is_null());
	for_each_voxel_metadata_in_area(box, [&callback](Vector3i pos, Variant meta) {
		const Variant key = pos.to_vec3();
		const Variant *args[2] = { &key, &meta };
		Variant::CallError err;
		callback->call_func(args, 2, err);

		ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK,
				String("FuncRef call failed at {0}").format(varray(key)));
		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
	});
}

void VoxelBufferInternal::clear_voxel_metadata() {
	_voxel_metadata.clear();
}

void VoxelBufferInternal::clear_voxel_metadata_in_area(Box3i box) {
	Map<Vector3i, Variant>::Element *elem = _voxel_metadata.front();
	while (elem != nullptr) {
		Map<Vector3i, Variant>::Element *next_elem = elem->next();
		if (box.contains(elem->key())) {
			_voxel_metadata.erase(elem);
		}
		elem = next_elem;
	}
}

void VoxelBufferInternal::copy_voxel_metadata_in_area(const VoxelBufferInternal &src_buffer, Box3i src_box,
		Vector3i dst_origin) {
	ERR_FAIL_COND(!src_buffer.is_box_valid(src_box));

	const Box3i clipped_src_box = src_box.clipped(Box3i(src_box.pos - dst_origin, _size));
	const Vector3i clipped_dst_offset = dst_origin + clipped_src_box.pos - src_box.pos;

	const Map<Vector3i, Variant>::Element *elem = src_buffer._voxel_metadata.front();

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

void VoxelBufferInternal::copy_voxel_metadata(const VoxelBufferInternal &src_buffer) {
	ERR_FAIL_COND(src_buffer.get_size() != _size);

	const Map<Vector3i, Variant>::Element *elem = src_buffer._voxel_metadata.front();

	while (elem != nullptr) {
		const Vector3i pos = elem->key();
		_voxel_metadata[pos] = elem->value().duplicate();
		elem = elem->next();
	}

	_block_metadata = src_buffer._block_metadata.duplicate();
}

Ref<Image> VoxelBufferInternal::debug_print_sdf_to_image_top_down() {
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
