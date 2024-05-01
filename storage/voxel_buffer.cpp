#include "voxel_buffer.h"
#include "../constants/voxel_constants.h"
#include "../util/containers/container_funcs.h"
#include "../util/containers/dynamic_bitset.h"
#include "../util/dstack.h"
#include "../util/profiling.h"
#include "../util/string/format.h"
#include "materials_4i4w.h"
#include "voxel_memory_pool.h"
#include <cstring>

namespace zylann::voxel {

inline uint8_t *allocate_channel_data(size_t size, VoxelBuffer::Allocator allocator) {
	ZN_DSTACK();
	switch (allocator) {
		case VoxelBuffer::ALLOCATOR_POOL:
			return VoxelMemoryPool::get_singleton().allocate(size);
		case VoxelBuffer::ALLOCATOR_DEFAULT:
			return (uint8_t *)ZN_ALLOC(size * sizeof(uint8_t));
		default:
			ZN_CRASH();
			return nullptr;
	}
}

inline void free_channel_data(uint8_t *data, uint32_t size, VoxelBuffer::Allocator allocator) {
	switch (allocator) {
		case VoxelBuffer::ALLOCATOR_POOL:
			VoxelMemoryPool::get_singleton().recycle(data, size);
			break;
		case VoxelBuffer::ALLOCATOR_DEFAULT:
			ZN_FREE(data);
			break;
		default:
			ZN_CRASH();
	}
}

// uint64_t g_depth_max_values[] = {
// 	0xff, // 8
// 	0xffff, // 16
// 	0xffffffff, // 32
// 	0xffffffffffffffff // 64
// };

// inline uint64_t get_max_value_for_depth(VoxelBuffer::Depth d) {
// 	CRASH_COND(d < 0 || d >= VoxelBuffer::DEPTH_COUNT);
// 	return g_depth_max_values[d];
// }

// inline uint64_t clamp_value_for_depth(uint64_t value, VoxelBuffer::Depth d) {
// 	const uint64_t max_val = get_max_value_for_depth(d);
// 	if (value >= max_val) {
// 		return max_val;
// 	}
// 	return value;
// }

static_assert(sizeof(uint32_t) == sizeof(float), "uint32_t and float cannot be marshalled back and forth");
static_assert(sizeof(uint64_t) == sizeof(double), "uint64_t and double cannot be marshalled back and forth");

union MarshallFloat {
	float f;
	uint32_t i;
};

union MarshallDouble {
	double d;
	uint64_t l;
};

inline uint64_t real_to_raw_voxel(real_t value, VoxelBuffer::Depth depth) {
	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			return snorm_to_s8(value * constants::QUANTIZED_SDF_8_BITS_SCALE);

		case VoxelBuffer::DEPTH_16_BIT:
			return snorm_to_s16(value * constants::QUANTIZED_SDF_16_BITS_SCALE);

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
			return s8_to_snorm(value) * constants::QUANTIZED_SDF_8_BITS_SCALE_INV;

		case VoxelBuffer::DEPTH_16_BIT:
			return s16_to_snorm(value) * constants::QUANTIZED_SDF_16_BITS_SCALE_INV;

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

namespace {
const uint64_t g_default_values[VoxelBuffer::MAX_CHANNELS] = {
	0, // TYPE

	// Casted explicitly to avoid warning about narrowing conversion, the intent is to store all bits of the value
	// as-is in a type that can store them all. The interpretation of the type is meaningless (depends on its use). It
	// should be possible to cast it back to the actual type with no loss of data, as long as all bits are preserved.
	uint16_t(snorm_to_s16(1.f)), // SDF

	0, // COLOR

	encode_indices_to_packed_u16(0, 1, 2, 3), // INDICES
	encode_weights_to_packed_u16_lossy(255, 0, 0, 0), // WEIGHTS

	0, 0, 0 //
};
}

uint64_t VoxelBuffer::get_default_value_static(unsigned int channel_index) {
	ZN_ASSERT(channel_index < MAX_CHANNELS);
	return g_default_values[channel_index];
}

// VoxelBuffer::VoxelBuffer() {
// 	init_channel_defaults();
// }

VoxelBuffer::VoxelBuffer(Allocator allocator) {
	init_channel_defaults();
	_allocator = allocator;
}

VoxelBuffer::VoxelBuffer(VoxelBuffer &&src) {
	src.move_to(*this);
}

VoxelBuffer::~VoxelBuffer() {
	clear();
}

void VoxelBuffer::init_channel_defaults() {
	// By default the buffer is all COMPRESSION_UNIFORM, only one value represented in each channel.

	// Minecraft uses way more than 255 block types and there is room for eventual metadata such as rotation
	_channels[CHANNEL_TYPE].depth = DEFAULT_TYPE_CHANNEL_DEPTH;
	_channels[CHANNEL_TYPE].defval = 0;

	// 16-bit is better on average to handle large worlds
	_channels[CHANNEL_SDF].depth = DEFAULT_SDF_CHANNEL_DEPTH;
	_channels[CHANNEL_SDF].defval = g_default_values[CHANNEL_SDF];

	_channels[CHANNEL_INDICES].depth = DEPTH_16_BIT;
	_channels[CHANNEL_INDICES].defval = g_default_values[CHANNEL_INDICES];

	_channels[CHANNEL_WEIGHTS].depth = DEPTH_16_BIT;
	_channels[CHANNEL_WEIGHTS].defval = g_default_values[CHANNEL_WEIGHTS];
}

VoxelBuffer &VoxelBuffer::operator=(VoxelBuffer &&src) {
	src.move_to(*this);
	return *this;
}

void VoxelBuffer::create(unsigned int sx, unsigned int sy, unsigned int sz) {
	ZN_DSTACK();
	ZN_ASSERT_RETURN(sx <= MAX_SIZE && sy <= MAX_SIZE && sz <= MAX_SIZE);

	// Always clear everything even if size doesn't change, because at least we want to start from default.
	// If one day we really want some hypothetic performance trying to re-use previously allocated data,
	// we could add a `create_no_reset` method.
	clear();

#ifdef TOOLS_ENABLED
	if (sx == 0 || sy == 0 || sz == 0) {
		ZN_PRINT_WARNING(format(
				"VoxelBuffer::create called with empty size ({}, {}, {}). It will be cleared instead.", sx, sy, sz));
		return;
	}
#endif

	_size = Vector3i(sx, sy, sz);

#ifdef DEV_ENABLED
	for (const Channel &channel : _channels) {
		ZN_ASSERT(channel.compression == COMPRESSION_UNIFORM);
	}
#endif
	// _allocator = allocator;
}

void VoxelBuffer::create(Vector3i size) {
	create(size.x, size.y, size.z);
}

void VoxelBuffer::clear() {
	for (unsigned int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {
		Channel &channel = _channels[channel_index];
		if (channel.compression != COMPRESSION_UNIFORM) {
			delete_channel(channel_index);
		}
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.compression == COMPRESSION_UNIFORM);
#endif
		// Reset voxel values to defaults
		channel.defval = g_default_values[channel_index];
	}
	_size = Vector3i();
	clear_voxel_metadata();
}

void VoxelBuffer::clear_channel(unsigned int channel_index, uint64_t clear_value) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	Channel &channel = _channels[channel_index];
	clear_channel(channel, clear_value, _allocator);
}

void VoxelBuffer::clear_channel(Channel &channel, uint64_t clear_value, Allocator allocator) {
	if (channel.compression != COMPRESSION_UNIFORM) {
		delete_channel(channel, allocator);
	}
	channel.defval = clear_value;
}

void VoxelBuffer::clear_channel_f(unsigned int channel_index, real_t clear_value) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	clear_channel(channel_index, real_to_raw_voxel(clear_value, channel.depth));
}

void VoxelBuffer::set_default_values(FixedArray<uint64_t, VoxelBuffer::MAX_CHANNELS> values) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		_channels[i].defval = values[i];
	}
}

uint64_t VoxelBuffer::get_voxel(int x, int y, int z, unsigned int channel_index) const {
	ZN_ASSERT_RETURN_V(channel_index < MAX_CHANNELS, 0);
	ZN_ASSERT_RETURN_V_MSG(is_position_valid(x, y, z), 0, format("Invalid position ({}, {}, {})", x, y, z));

	const Channel &channel = _channels[channel_index];

	if (channel.compression == COMPRESSION_UNIFORM) {
		return channel.defval;

	} else {
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.data != nullptr);
#endif
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
	}
}

void VoxelBuffer::set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index) {
	ZN_DSTACK();
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	ZN_ASSERT_RETURN_MSG(is_position_valid(x, y, z), format("At position ({}, {}, {})", x, y, z));

	Channel &channel = _channels[channel_index];

	bool do_set = true;

	if (channel.compression == COMPRESSION_UNIFORM) {
		if (channel.defval != value) {
			// Allocate channel with same initial values as defval
			ZN_ASSERT_RETURN(create_channel(channel_index, channel.defval));
		} else {
			do_set = false;
		}
	}

	if (do_set) {
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.data != nullptr);
#endif

		const uint32_t i = get_index(x, y, z);

		switch (channel.depth) {
			case DEPTH_8_BIT:
				// Note, if the value is negative, it may be in the range supported by int8_t.
				// This use case might exist for SDF data, although it is preferable to use `set_voxel_f`.
				// Similar for higher depths.
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
	ZN_ASSERT_RETURN_V(channel_index < MAX_CHANNELS, 0);
	return raw_voxel_to_real(get_voxel(x, y, z, channel_index), _channels[channel_index].depth);
}

void VoxelBuffer::set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	set_voxel(real_to_raw_voxel(value, _channels[channel_index].depth), x, y, z, channel_index);
}

void VoxelBuffer::fill(uint64_t defval, unsigned int channel_index) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

	Channel &channel = _channels[channel_index];

	if (channel.compression == COMPRESSION_UNIFORM) {
		// Channel is already optimized and uniform
		if (channel.defval == defval) {
			// No change
		} else {
			// Just change default value
			channel.defval = defval;
		}
		return;
	}

	const size_t volume = get_volume();
#ifdef DEBUG_ENABLED
	ZN_ASSERT(channel.size_in_bytes == get_size_in_bytes_for_volume(_size, channel.depth));
#endif
#ifdef DEV_ENABLED
	ZN_ASSERT(channel.data != nullptr);
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

void VoxelBuffer::fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index) {
	ZN_DSTACK();
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

	Vector3iUtil::sort_min_max(min, max);
	min = min.clamp(Vector3i(0, 0, 0), _size);
	max = max.clamp(Vector3i(0, 0, 0), _size); // `_size` is included
	const Vector3i area_size = max - min;
	if (area_size.x == 0 || area_size.y == 0 || area_size.z == 0) {
		return;
	}

	Channel &channel = _channels[channel_index];

	if (channel.compression == COMPRESSION_UNIFORM) {
		if (channel.defval == defval) {
			return;
		} else {
			ZN_ASSERT_RETURN(create_channel(channel_index, channel.defval));
		}
	}

#ifdef DEV_ENABLED
	ZN_ASSERT(channel.data != nullptr);
#endif

	Vector3i pos;
	const size_t volume = get_volume();

	for (pos.z = min.z; pos.z < max.z; ++pos.z) {
		for (pos.x = min.x; pos.x < max.x; ++pos.x) {
			const size_t dst_ri = get_index(pos.x, pos.y + min.y, pos.z);
			ZN_ASSERT(dst_ri < volume);

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
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	const Channel &channel = _channels[channel_index];
	fill_area(real_to_raw_voxel(fvalue, channel.depth), min, max, channel_index);
}

void VoxelBuffer::fill_f(real_t value, unsigned int channel) {
	ZN_ASSERT_RETURN(channel < MAX_CHANNELS);
	fill(real_to_raw_voxel(value, _channels[channel].depth), channel);
}

template <typename T>
inline bool is_uniform_b(const uint8_t *data, size_t item_count) {
	return is_uniform<T>(reinterpret_cast<const T *>(data), item_count);
}

bool VoxelBuffer::is_uniform(unsigned int channel_index) const {
	ZN_ASSERT_RETURN_V(channel_index < MAX_CHANNELS, true);
	const Channel &channel = _channels[channel_index];
	return is_uniform(channel);
}

bool VoxelBuffer::is_uniform(const Channel &channel) {
	if (channel.compression == COMPRESSION_UNIFORM) {
		// Channel has been optimized
		return true;
	}

	// Channel isn't optimized, so must look at each voxel
	switch (channel.depth) {
		case DEPTH_8_BIT:
			return is_uniform_b<uint8_t>(channel.data, channel.size_in_bytes);
		case DEPTH_16_BIT:
			return is_uniform_b<uint16_t>(channel.data, channel.size_in_bytes / 2);
		case DEPTH_32_BIT:
			return is_uniform_b<uint32_t>(channel.data, channel.size_in_bytes / 4);
		case DEPTH_64_BIT:
			return is_uniform_b<uint64_t>(channel.data, channel.size_in_bytes / 8);
		default:
			CRASH_NOW();
			break;
	}

	return true;
}

uint64_t get_first_voxel(const VoxelBuffer::Channel &channel) {
	ZN_ASSERT(channel.compression != VoxelBuffer::COMPRESSION_UNIFORM);
#ifdef DEV_ENABLED
	ZN_ASSERT(channel.data != nullptr);
#endif

	switch (channel.depth) {
		case VoxelBuffer::DEPTH_8_BIT:
			return channel.data[0];

		case VoxelBuffer::DEPTH_16_BIT:
			return reinterpret_cast<uint16_t *>(channel.data)[0];

		case VoxelBuffer::DEPTH_32_BIT:
			return reinterpret_cast<uint32_t *>(channel.data)[0];

		case VoxelBuffer::DEPTH_64_BIT:
			return reinterpret_cast<uint64_t *>(channel.data)[0];

		default:
			ZN_CRASH_MSG("Unexpected depth");
			return 0;
	}
}

void VoxelBuffer::compress_uniform_channels() {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		Channel &channel = _channels[i];
		compress_if_uniform(channel);
	}
}

void VoxelBuffer::compress_if_uniform(Channel &channel) {
	if (channel.compression != COMPRESSION_UNIFORM && is_uniform(channel)) {
		const uint64_t v = get_first_voxel(channel);
		clear_channel(channel, v, _allocator);
	}
}

void VoxelBuffer::decompress_channel(unsigned int channel_index) {
	ZN_DSTACK();
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	ZN_ASSERT_RETURN(!Vector3iUtil::is_empty_size(get_size()));
	Channel &channel = _channels[channel_index];
	if (channel.compression == COMPRESSION_UNIFORM) {
		ZN_ASSERT_RETURN(create_channel(channel_index, channel.defval));
	}
}

VoxelBuffer::Compression VoxelBuffer::get_channel_compression(unsigned int channel_index) const {
	ZN_ASSERT_RETURN_V(channel_index < MAX_CHANNELS, VoxelBuffer::COMPRESSION_NONE);
	const Channel &channel = _channels[channel_index];
	return channel.compression;
}

void VoxelBuffer::copy_format(const VoxelBuffer &other) {
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		set_channel_depth(i, other.get_channel_depth(i));
	}
}

void VoxelBuffer::copy_channels_from(const VoxelBuffer &other) {
	// Copy all channels, assuming sizes and formats match
	for (unsigned int i = 0; i < MAX_CHANNELS; ++i) {
		copy_channel_from(other, i);
	}
}

void VoxelBuffer::copy_channel_from(const VoxelBuffer &other, unsigned int channel_index) {
	ZN_DSTACK();
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	ZN_ASSERT_RETURN(other._size == _size);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ZN_ASSERT_RETURN(other_channel.depth == channel.depth);

	if (other_channel.compression != COMPRESSION_UNIFORM) {
		// Other is not uniform, make sure we allocate our channel
		if (channel.compression == COMPRESSION_UNIFORM) {
			ZN_ASSERT_RETURN(create_channel_noinit(channel_index, _size));
		}
		ZN_ASSERT(channel.size_in_bytes == other_channel.size_in_bytes);
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.data != nullptr);
		ZN_ASSERT(other_channel.data != nullptr);
#endif
		memcpy(channel.data, other_channel.data, channel.size_in_bytes);

	} else {
		// Other is uniform, deallocate our channel too
		if (channel.compression != COMPRESSION_UNIFORM) {
			delete_channel(channel_index);
		}
		channel.defval = other_channel.defval;
	}

	// Not really necessary since we already require depths to be equal?
	channel.depth = other_channel.depth;

#ifdef DEV_ENABLED
	ZN_ASSERT(channel.compression == other_channel.compression);
#endif
}

// TODO Disallow copying from overlapping areas of the same buffer
void VoxelBuffer::copy_channel_from(
		const VoxelBuffer &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min, unsigned int channel_index) {
	//
	ZN_DSTACK();
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);

	Channel &channel = _channels[channel_index];
	const Channel &other_channel = other._channels[channel_index];

	ZN_ASSERT_RETURN(other_channel.depth == channel.depth);

	if (channel.compression == COMPRESSION_UNIFORM && other_channel.compression == COMPRESSION_UNIFORM &&
			channel.defval == other_channel.defval) {
		// No action needed
		return;
	}

	if (other_channel.compression != COMPRESSION_UNIFORM) {
		if (channel.compression == COMPRESSION_UNIFORM) {
			// Note, we do this even if the pasted data happens to be all the same value as our current channel.
			// We assume that this case is not frequent enough to bother, and compression can happen later
			ZN_ASSERT_RETURN(create_channel(channel_index, channel.defval));
		}
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.data != nullptr);
		ZN_ASSERT(other_channel.data != nullptr);
#endif
		const unsigned int item_size = get_depth_byte_count(channel.depth);
		Span<const uint8_t> src(other_channel.data, other_channel.size_in_bytes);
		Span<uint8_t> dst(channel.data, channel.size_in_bytes);
		copy_3d_region_zxy(dst, _size, dst_min, src, other._size, src_min, src_max, item_size);

	} else if (channel.defval != other_channel.defval) {
		// Other is uniform, but we are not, and we copy an area so we can't assume to become uniform too.

		// This logic is still required due to how source and destination regions can be specified.
		// The actual size of the destination area must be determined from the source area, after it has been clipped.
		Vector3iUtil::sort_min_max(src_min, src_max);
		clip_copy_region(src_min, src_max, other._size, dst_min, _size);
		const Vector3i area_size = src_max - src_min;
		if (area_size.x <= 0 || area_size.y <= 0 || area_size.z <= 0) {
			// Degenerate area, we'll not copy anything.
			return;
		}
		fill_area(other_channel.defval, dst_min, dst_min + area_size, channel_index);
	}
}

void VoxelBuffer::copy_to(VoxelBuffer &dst, bool include_metadata) const {
	ZN_DSTACK();
	dst.create(_size);
	for (unsigned int i = 0; i < _channels.size(); ++i) {
		dst.set_channel_depth(i, _channels[i].depth);
	}
	dst.copy_channels_from(*this);
	if (include_metadata) {
		dst.copy_voxel_metadata(*this);
	}
}

void VoxelBuffer::move_to(VoxelBuffer &dst) {
	if (this == &dst) {
		ZN_PRINT_VERBOSE("Moving VoxelBuffer to itself?");
		return;
	}

	dst.clear();

	dst._channels = _channels;
	dst._size = _size;
	dst._allocator = _allocator;

	dst._block_metadata = std::move(_block_metadata);
	dst._voxel_metadata = std::move(_voxel_metadata);

	for (unsigned int i = 0; i < _channels.size(); ++i) {
		Channel &channel = _channels[i];
		channel.data = nullptr;
		channel.compression = COMPRESSION_UNIFORM;
		channel.size_in_bytes = 0;
	}
}

bool VoxelBuffer::get_channel_raw(unsigned int channel_index, Span<uint8_t> &slice) const {
	const Channel &channel = _channels[channel_index];
	if (channel.compression != COMPRESSION_UNIFORM) {
#ifdef DEV_ENABLED
		ZN_ASSERT(channel.data != nullptr);
#endif
		slice = Span<uint8_t>(channel.data, 0, channel.size_in_bytes);
		return true;
	}
	// TODO Could we just return `Span<uint8_t>(&channel.defval, 1)` alongside the `false` return?
	slice = Span<uint8_t>();
	return false;
}

bool VoxelBuffer::get_channel_raw_read_only(unsigned int channel_index, Span<const uint8_t> &slice) const {
	Span<uint8_t> slice_w;
	const bool success = get_channel_raw(channel_index, slice_w);
	slice = slice_w;
	return success;
}

bool VoxelBuffer::create_channel(int i, uint64_t defval) {
	ZN_DSTACK();
	if (!create_channel_noinit(i, _size)) {
		return false;
	}
	fill(defval, i);
	return true;
}

size_t VoxelBuffer::get_size_in_bytes_for_volume(Vector3i size, Depth depth) {
	// Calculate appropriate size based on bit depth
	const size_t volume = size.x * size.y * size.z;
	const size_t bits = volume * get_depth_bit_count(depth);
	const size_t size_in_bytes = (bits >> 3);
	return size_in_bytes;
}

bool VoxelBuffer::create_channel_noinit(int i, Vector3i size) {
	ZN_DSTACK();
	Channel &channel = _channels[i];
	const size_t size_in_bytes = get_size_in_bytes_for_volume(size, channel.depth);
	ZN_ASSERT_RETURN_V_MSG(size_in_bytes <= Channel::MAX_SIZE_IN_BYTES, false, "Buffer is too big");
	ZN_ASSERT(channel.compression == COMPRESSION_UNIFORM); // The channel must not already be allocated
	channel.data = allocate_channel_data(size_in_bytes, _allocator);
	ZN_ASSERT_RETURN_V(channel.data != nullptr, false); // Bad alloc?
	channel.compression = COMPRESSION_NONE;
	channel.size_in_bytes = size_in_bytes;
	return true;
}

void VoxelBuffer::delete_channel(int i) {
	Channel &channel = _channels[i];
	delete_channel(channel, _allocator);
}

void VoxelBuffer::delete_channel(Channel &channel, Allocator allocator) {
	ZN_ASSERT_RETURN(channel.compression != COMPRESSION_UNIFORM);
	// Don't use `_size` to obtain `data` byte count, since we could have changed `_size` up-front during a create().
	// `size_in_bytes` reflects what is currently allocated inside `data`, regardless of anything else.
	free_channel_data(channel.data, channel.size_in_bytes, allocator);
	channel.data = nullptr;
	channel.compression = COMPRESSION_UNIFORM;
	channel.size_in_bytes = 0;
}

void VoxelBuffer::downscale_to(VoxelBuffer &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const {
	// TODO Align input to multiple of two

	src_min = src_min.clamp(Vector3i(), _size - Vector3i(1, 1, 1));
	src_max = src_max.clamp(Vector3i(), _size);

	Vector3i dst_max = dst_min + ((src_max - src_min) >> 1);

	// TODO This will be wrong if it overlaps the border?
	dst_min = dst_min.clamp(Vector3i(), dst._size - Vector3i(1, 1, 1));
	dst_max = dst_max.clamp(Vector3i(), dst._size);

	for (int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {
		const Channel &src_channel = _channels[channel_index];
		const Channel &dst_channel = dst._channels[channel_index];

		if (src_channel.compression == COMPRESSION_UNIFORM && dst_channel.compression == COMPRESSION_UNIFORM &&
				src_channel.defval == dst_channel.defval) {
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
					ZN_ASSERT(is_position_valid(src_pos.x, src_pos.y, src_pos.z));

					uint64_t v;
					if (src_channel.compression != COMPRESSION_UNIFORM) {
						// TODO Optimized version?
						v = get_voxel(src_pos, channel_index);
					} else {
						v = src_channel.defval;
					}

					// TODO Could be optimized?
					dst.set_voxel(v, pos, channel_index);
				}
			}
		}
	}
}

bool VoxelBuffer::equals(const VoxelBuffer &p_other) const {
	if (p_other._size != _size) {
		return false;
	}

	for (int channel_index = 0; channel_index < MAX_CHANNELS; ++channel_index) {
		const Channel &channel = _channels[channel_index];
		const Channel &other_channel = p_other._channels[channel_index];

		if (channel.compression != other_channel.compression) {
			// Note: they could still logically be equal if one channel contains uniform voxel memory.
			return false;
		}

		if (channel.depth != other_channel.depth) {
			return false;
		}

		if (channel.compression == COMPRESSION_UNIFORM) {
			if (channel.defval != other_channel.defval) {
				return false;
			}

		} else {
			ZN_ASSERT_RETURN_V(channel.size_in_bytes == other_channel.size_in_bytes, false);
#ifdef DEV_ENABLED
			ZN_ASSERT(channel.data != nullptr);
			ZN_ASSERT(other_channel.data != nullptr);
#endif
			for (size_t i = 0; i < channel.size_in_bytes; ++i) {
				if (channel.data[i] != other_channel.data[i]) {
					return false;
				}
			}
		}
	}

	return true;
}

void VoxelBuffer::set_channel_depth(unsigned int channel_index, Depth new_depth) {
	ZN_ASSERT_RETURN(channel_index < MAX_CHANNELS);
	ZN_ASSERT_RETURN(new_depth >= 0 && new_depth < DEPTH_COUNT);
	Channel &channel = _channels[channel_index];
	if (channel.depth == new_depth) {
		return;
	}
	if (channel.compression != COMPRESSION_UNIFORM) {
		// TODO Implement conversion and do it when specified
		WARN_PRINT("Changing VoxelBuffer depth with present data, this will reset the channel");
		delete_channel(channel_index);
	}
	channel.depth = new_depth;
}

VoxelBuffer::Depth VoxelBuffer::get_channel_depth(unsigned int channel_index) const {
	ZN_ASSERT_RETURN_V(channel_index < MAX_CHANNELS, DEPTH_8_BIT);
	return _channels[channel_index].depth;
}

float VoxelBuffer::get_sdf_quantization_scale(Depth d) {
	switch (d) {
		// Normalized
		case DEPTH_8_BIT:
			return constants::QUANTIZED_SDF_8_BITS_SCALE;
		case DEPTH_16_BIT:
			return constants::QUANTIZED_SDF_16_BITS_SCALE;
		// Direct
		default:
			return 1.f;
	}
}

void VoxelBuffer::get_range_f(float &out_min, float &out_max, ChannelId channel_index) const {
	const Channel &channel = _channels[channel_index];
	float min_value = get_voxel_f(0, 0, 0, channel_index);
	float max_value = min_value;

	if (channel.compression == COMPRESSION_UNIFORM) {
		out_min = min_value;
		out_max = max_value;
		return;
	}

	const uint64_t volume = get_volume();

#ifdef DEV_ENABLED
	ZN_ASSERT(channel.data != nullptr);
#endif

	switch (channel.depth) {
		case DEPTH_8_BIT:
			for (unsigned int i = 0; i < volume; ++i) {
				const float v = s8_to_snorm(channel.data[i]);
				min_value = math::min(v, min_value);
				max_value = math::max(v, max_value);
			}
			break;
		case DEPTH_16_BIT: {
			const int16_t *data = reinterpret_cast<const int16_t *>(channel.data);
			for (unsigned int i = 0; i < volume; ++i) {
				const float v = s16_to_snorm(data[i]);
				min_value = math::min(v, min_value);
				max_value = math::max(v, max_value);
			}
		} break;
		case DEPTH_32_BIT: {
			const float *data = reinterpret_cast<const float *>(channel.data);
			for (unsigned int i = 0; i < volume; ++i) {
				const float v = data[i];
				min_value = math::min(v, min_value);
				max_value = math::max(v, max_value);
			}
		} break;
		case DEPTH_64_BIT: {
			const double *data = reinterpret_cast<const double *>(channel.data);
			for (unsigned int i = 0; i < volume; ++i) {
				const double v = data[i];
				min_value = math::min(v, double(min_value));
				max_value = math::max(v, double(max_value));
			}
		} break;
		default:
			CRASH_NOW();
	}

	const float q = get_sdf_quantization_scale(channel.depth);
	out_min = min_value * q;
	out_max = max_value * q;
}

const VoxelMetadata *VoxelBuffer::get_voxel_metadata(Vector3i pos) const {
	ZN_ASSERT_RETURN_V(is_position_valid(pos), nullptr);
	return _voxel_metadata.find(pos);
}

VoxelMetadata *VoxelBuffer::get_voxel_metadata(Vector3i pos) {
	ZN_ASSERT_RETURN_V(is_position_valid(pos), nullptr);
	return _voxel_metadata.find(pos);
}

VoxelMetadata *VoxelBuffer::get_or_create_voxel_metadata(Vector3i pos) {
	ZN_ASSERT_RETURN_V(is_position_valid(pos), nullptr);
	VoxelMetadata *d = _voxel_metadata.find(pos);
	if (d != nullptr) {
		return d;
	}
	// TODO Optimize: we know the key should not exist
	VoxelMetadata &meta = _voxel_metadata.insert_or_assign(pos, VoxelMetadata());
	return &meta;
}

void VoxelBuffer::erase_voxel_metadata(Vector3i pos) {
	ZN_ASSERT_RETURN(is_position_valid(pos));
	_voxel_metadata.erase(pos);
}

void VoxelBuffer::clear_and_set_voxel_metadata(Span<FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair> pairs) {
#ifdef DEBUG_ENABLED
	for (size_t i = 0; i < pairs.size(); ++i) {
		ZN_ASSERT_CONTINUE(is_position_valid(pairs[i].key));
	}
#endif
	_voxel_metadata.clear_and_insert(pairs);
}

/*#ifdef ZN_GODOT

void VoxelBuffer::for_each_voxel_metadata(const Callable &callback) const {
	ERR_FAIL_COND(callback.is_null());

	for (FlatMap<Vector3i, Variant>::ConstIterator it = _voxel_metadata.begin(); it != _voxel_metadata.end(); ++it) {
		const Variant key = it->key;
		const Variant *args[2] = { &key, &it->value };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.call(args, 2, retval, err);

		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key)));
		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
	}
}

void VoxelBuffer::for_each_voxel_metadata_in_area(const Callable &callback, Box3i box) const {
	ERR_FAIL_COND(callback.is_null());
	for_each_voxel_metadata_in_area(box, [&callback](Vector3i pos, Variant meta) {
		const Variant key = pos;
		const Variant *args[2] = { &key, &meta };
		Callable::CallError err;
		Variant retval; // We don't care about the return value, Callable API requires it
		callback.call(args, 2, retval, err);

		ERR_FAIL_COND_MSG(
				err.error != Callable::CallError::CALL_OK, String("Callable failed at {0}").format(varray(key)));
		// TODO Can't provide detailed error because FuncRef doesn't give us access to the object
		// ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK, false,
		// 		Variant::get_call_error_text(callback->get_object(), method_name, nullptr, 0, err));
	});
}

#endif*/

void VoxelBuffer::clear_voxel_metadata() {
	_voxel_metadata.clear();
}

void VoxelBuffer::clear_voxel_metadata_in_area(Box3i box) {
	_voxel_metadata.remove_if([&box](const FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair &p) { //
		return box.contains(p.key);
	});
}

void VoxelBuffer::copy_voxel_metadata_in_area(const VoxelBuffer &src_buffer, Box3i src_box, Vector3i dst_origin) {
	ZN_ASSERT_RETURN(src_buffer.is_box_valid(src_box));

	const Box3i clipped_src_box = src_box.clipped(Box3i(src_box.position - dst_origin, _size));
	const Vector3i clipped_dst_offset = dst_origin + clipped_src_box.position - src_box.position;

	for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator src_it = src_buffer._voxel_metadata.begin();
			src_it != src_buffer._voxel_metadata.end(); ++src_it) {
		if (src_box.contains(src_it->key)) {
			const Vector3i dst_pos = src_it->key + clipped_dst_offset;
			ZN_ASSERT(is_position_valid(dst_pos));

			VoxelMetadata &meta = _voxel_metadata.insert_or_assign(dst_pos, VoxelMetadata());
			meta.copy_from(src_it->value);
		}
	}
}

void VoxelBuffer::copy_voxel_metadata(const VoxelBuffer &src_buffer) {
	ZN_ASSERT_RETURN(src_buffer.get_size() == _size);

	for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator src_it = src_buffer._voxel_metadata.begin();
			src_it != src_buffer._voxel_metadata.end(); ++src_it) {
		VoxelMetadata &meta = _voxel_metadata.insert_or_assign(src_it->key, VoxelMetadata());
		meta.copy_from(src_it->value);
	}

	_block_metadata.copy_from(src_buffer._block_metadata);
}

void get_unscaled_sdf(const VoxelBuffer &voxels, Span<float> sdf) {
	ZN_PROFILE_SCOPE();
	ZN_DSTACK();
	const uint64_t volume = Vector3iUtil::get_volume(voxels.get_size());
	ZN_ASSERT_RETURN(volume == sdf.size());

	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::Depth depth = voxels.get_channel_depth(channel);

	if (voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_UNIFORM) {
		const float uniform_value = voxels.get_voxel_f(0, 0, 0, channel);
		sdf.fill(uniform_value);
		return;
	}

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s8_to_snorm(raw[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = s16_to_snorm(raw[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(sdf.data(), raw.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				sdf[i] = raw[i];
			}
		} break;

		default:
			ZN_CRASH();
	}

	const float inv_scale = 1.f / VoxelBuffer::get_sdf_quantization_scale(depth);

	for (float &sd : sdf) {
		sd *= inv_scale;
	}
}

void scale_and_store_sdf(VoxelBuffer &voxels, Span<float> sdf) {
	ZN_PROFILE_SCOPE();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::Depth depth = voxels.get_channel_depth(channel);
	ZN_ASSERT_RETURN(voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_NONE);

	const float scale = VoxelBuffer::get_sdf_quantization_scale(depth);
	for (unsigned int i = 0; i < sdf.size(); ++i) {
		sdf[i] *= scale;
	}

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = snorm_to_s8(sdf[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = snorm_to_s16(sdf[i]);
			}
		} break;

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(raw.data(), sdf.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = sdf[i];
			}
		} break;

		default:
			ZN_CRASH();
	}
}

void scale_and_store_sdf_if_modified(VoxelBuffer &voxels, Span<float> sdf, Span<const float> comparand) {
	ZN_PROFILE_SCOPE();
	const VoxelBuffer::ChannelId channel = VoxelBuffer::CHANNEL_SDF;
	const VoxelBuffer::Depth depth = voxels.get_channel_depth(channel);
	ZN_ASSERT_RETURN(voxels.get_channel_compression(channel) == VoxelBuffer::COMPRESSION_NONE);

	const float scale = VoxelBuffer::get_sdf_quantization_scale(depth);
	// for (unsigned int i = 0; i < sdf.size(); ++i) {
	// 	sdf[i] *= scale;
	// }

	switch (depth) {
		case VoxelBuffer::DEPTH_8_BIT: {
			Span<int8_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				if (comparand[i] != sdf[i]) {
					raw[i] = snorm_to_s8(sdf[i] * scale);
				}
			}
		} break;

		case VoxelBuffer::DEPTH_16_BIT: {
			Span<int16_t> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				if (comparand[i] != sdf[i]) {
					raw[i] = snorm_to_s16(sdf[i] * scale);
				}
			}
		} break;

			// Float formats don't need this, so they are still fully written.

		case VoxelBuffer::DEPTH_32_BIT: {
			Span<float> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			memcpy(raw.data(), sdf.data(), sizeof(float) * sdf.size());
		} break;

		case VoxelBuffer::DEPTH_64_BIT: {
			Span<double> raw;
			ZN_ASSERT(voxels.get_channel_data(channel, raw));
			for (unsigned int i = 0; i < sdf.size(); ++i) {
				raw[i] = sdf[i];
			}
		} break;

		default:
			ZN_CRASH();
	}
}

void paste(Span<const uint8_t> channels, //
		const VoxelBuffer &src_buffer, //
		VoxelBuffer &dst_buffer, //
		const Vector3i dst_base_pos, //
		bool with_metadata //
) {
	for (const uint8_t channel : channels) {
		dst_buffer.copy_channel_from(src_buffer, Vector3i(), src_buffer.get_size(), dst_base_pos, channel);
	}

	if (with_metadata) {
		const Box3i dst_box(dst_base_pos, src_buffer.get_size());
		dst_buffer.clear_voxel_metadata_in_area(dst_box);
		dst_buffer.copy_voxel_metadata_in_area(src_buffer, Box3i(Vector3i(), src_buffer.get_size()), dst_base_pos);
	}
}

void paste_src_masked(Span<const uint8_t> channels, //
		const VoxelBuffer &src_buffer, //
		unsigned int src_mask_channel, //
		uint64_t src_mask_value, //
		VoxelBuffer &dst_buffer, //
		const Vector3i dst_base_pos, //
		bool with_metadata //
) {
	const Box3i dst_box = Box3i(dst_base_pos, src_buffer.get_size()).clipped(dst_buffer.get_size());

	for (const uint8_t channel : channels) {
		if (channel == src_mask_channel) {
			dst_buffer.read_write_action(dst_box, channel,
					[&src_buffer, src_mask_value, dst_base_pos, channel](const Vector3i pos, uint64_t dst_v) {
						const uint64_t src_v = src_buffer.get_voxel(pos - dst_base_pos, channel);
						if (src_v == src_mask_value) {
							return dst_v;
						}
						return src_v;
					});
		} else {
			dst_buffer.read_write_action(dst_box, channel,
					[&src_buffer, src_mask_value, dst_base_pos, channel, src_mask_channel](
							const Vector3i pos, uint64_t dst_v) {
						const uint64_t mv = src_buffer.get_voxel(pos - dst_base_pos, src_mask_channel);
						if (mv == src_mask_value) {
							return dst_v;
						}
						const uint64_t src_v = src_buffer.get_voxel(pos - dst_base_pos, channel);
						return src_v;
					});
		}
	}

	if (with_metadata) {
		dst_buffer.erase_voxel_metadata_if([dst_box, &src_buffer, src_mask_channel, src_mask_value](
												   const FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair &p) {
			return dst_box.contains(p.key) && src_buffer.get_voxel(p.key, src_mask_channel) != src_mask_value;
		});

		const Box3i src_box(dst_box.position - dst_base_pos, dst_box.size);

		src_buffer.for_each_voxel_metadata_in_area(src_box,
				[&src_buffer, src_mask_channel, src_mask_value, &dst_buffer, dst_box, dst_base_pos](
						Vector3i src_pos, const VoxelMetadata &src_meta) {
					const Vector3i dst_pos = src_pos + dst_base_pos;
					if (dst_box.contains(dst_pos) &&
							src_buffer.get_voxel(src_pos, src_mask_channel) != src_mask_value) {
						VoxelMetadata *dst_meta = dst_buffer.get_or_create_voxel_metadata(dst_pos);
#ifdef TOOLS_ENABLED
						ZN_ASSERT_RETURN(dst_meta != nullptr);
#endif
						dst_meta->copy_from(src_meta);
					}
				});
	}
}

// Paste if the source is not a certain value, and the destination satisfies a predicate
template <typename FDstPredicate>
void paste_src_masked_dst_predicate(Span<const uint8_t> channels, //
		const VoxelBuffer &src_buffer, //
		unsigned int src_mask_channel, //
		uint64_t src_mask_value, //
		VoxelBuffer &dst_buffer, //
		const Vector3i dst_base_pos, //
		unsigned int dst_mask_channel, //
		FDstPredicate dst_predicate, //
		bool with_metadata //
) {
	const Box3i dst_box = Box3i(dst_base_pos, src_buffer.get_size()).clipped(dst_buffer.get_size());

	for (const uint8_t channel : channels) {
		if (channel == src_mask_channel && channel == dst_mask_channel) {
			// Common path for blocky games
			dst_buffer.read_write_action(dst_box, channel,
					[&src_buffer, src_mask_value, dst_base_pos, channel, &dst_predicate](
							const Vector3i pos, uint64_t dst_v) {
						const uint64_t src_v = src_buffer.get_voxel(pos - dst_base_pos, channel);
						if (src_v == src_mask_value) {
							return dst_v;
						}
						if (!dst_predicate(dst_v)) {
							return dst_v;
						}
						return src_v;
					});

		} else {
			dst_buffer.read_write_action(dst_box, channel,
					[&src_buffer, src_mask_value, dst_base_pos, channel, src_mask_channel, &dst_buffer, &dst_predicate,
							dst_mask_channel](const Vector3i pos, uint64_t dst_v) {
						const uint64_t src_mv = src_buffer.get_voxel(pos - dst_base_pos, src_mask_channel);
						if (src_mv == src_mask_value) {
							return dst_v;
						}
						const uint64_t dst_mv = dst_buffer.get_voxel(pos, dst_mask_channel);
						if (!dst_predicate(dst_mv)) {
							return dst_v;
						}
						const uint64_t src_v = src_buffer.get_voxel(pos - dst_base_pos, channel);
						return src_v;
					});
		}
	}

	if (with_metadata) {
		dst_buffer.erase_voxel_metadata_if(
				[&src_buffer, src_mask_channel, src_mask_value, dst_box, &dst_buffer, dst_mask_channel, &dst_predicate](
						const FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair &p) {
					//
					return dst_box.contains(p.key) //
							&& src_buffer.get_voxel(p.key, src_mask_channel) != src_mask_value //
							&& dst_predicate(dst_buffer.get_voxel(p.key, dst_mask_channel));
				});

		const Box3i src_box(dst_box.position - dst_base_pos, dst_box.size);

		src_buffer.for_each_voxel_metadata_in_area(src_box,
				[&src_buffer, src_mask_channel, src_mask_value, &dst_buffer, dst_box, dst_base_pos, dst_mask_channel,
						&dst_predicate](Vector3i src_pos, const VoxelMetadata &src_meta) {
					//
					const Vector3i dst_pos = src_pos + dst_base_pos;
					if (dst_box.contains(dst_pos) //
							&& src_buffer.get_voxel(src_pos, src_mask_channel) != src_mask_value //
							&& dst_predicate(dst_buffer.get_voxel(dst_pos, dst_mask_channel))) {
						VoxelMetadata *dst_meta = dst_buffer.get_or_create_voxel_metadata(dst_pos);
#ifdef TOOLS_ENABLED
						ZN_ASSERT_RETURN(dst_meta != nullptr);
#endif
						dst_meta->copy_from(src_meta);
					}
				});
	}
}

void paste_src_masked_dst_writable_value(Span<const uint8_t> channels, //
		const VoxelBuffer &src_buffer, //
		unsigned int src_mask_channel, //
		uint64_t src_mask_value, //
		VoxelBuffer &dst_buffer, //
		const Vector3i dst_base_pos, //
		unsigned int dst_mask_channel, //
		uint64_t dst_mask_value, //
		bool with_metadata //
) {
	paste_src_masked_dst_predicate(
			channels,
			src_buffer, //
			src_mask_channel, //
			src_mask_value, //
			dst_buffer, //
			dst_base_pos, //
			dst_mask_channel, //
			[dst_mask_value](const uint64_t dst_v) { //
				return dst_v == dst_mask_value;
			},
			with_metadata);
}

void paste_src_masked_dst_writable_bitarray(Span<const uint8_t> channels, //
		const VoxelBuffer &src_buffer, //
		unsigned int src_mask_channel, //
		uint64_t src_mask_value, //
		VoxelBuffer &dst_buffer, //
		const Vector3i dst_base_pos, //
		unsigned int dst_mask_channel, //
		const DynamicBitset &bitarray, //
		bool with_metadata //
) {
	paste_src_masked_dst_predicate(
			channels,
			src_buffer, //
			src_mask_channel, //
			src_mask_value, //
			dst_buffer, //
			dst_base_pos, //
			dst_mask_channel, //
			[&bitarray](const uint64_t dst_v) { //
				return dst_v < bitarray.size() && bitarray.get(dst_v);
			},
			with_metadata);
}

} // namespace zylann::voxel
