#ifndef VOXEL_BUFFER_INTERNAL_H
#define VOXEL_BUFFER_INTERNAL_H

#include "../constants/voxel_constants.h"
#include "../util/fixed_array.h"
#include "../util/math/box3i.h"
#include "../util/span.h"
#include "funcs.h"

#include <core/map.h>
#include <core/reference.h>
#include <core/vector.h>

class VoxelTool;
class Image;
class FuncRef;

// Dense voxels data storage.
// Organized in channels of configurable bit depth.
// Values can be interpreted either as unsigned integers or normalized floats.
class VoxelBufferInternal {
public:
	enum ChannelId {
		CHANNEL_TYPE = 0,
		CHANNEL_SDF,
		CHANNEL_COLOR,
		CHANNEL_INDICES,
		CHANNEL_WEIGHTS,
		CHANNEL_DATA5,
		CHANNEL_DATA6,
		CHANNEL_DATA7,
		// Arbitrary value, 8 should be enough. Tweak for your needs.
		MAX_CHANNELS
	};

	static const int ALL_CHANNELS_MASK = 0xff;

	enum Compression {
		COMPRESSION_NONE = 0,
		COMPRESSION_UNIFORM,
		//COMPRESSION_RLE,
		COMPRESSION_COUNT
	};

	enum Depth {
		DEPTH_8_BIT,
		DEPTH_16_BIT,
		DEPTH_32_BIT,
		DEPTH_64_BIT,
		DEPTH_COUNT
	};

	static inline uint32_t get_depth_byte_count(VoxelBufferInternal::Depth d) {
		CRASH_COND(d < 0 || d >= VoxelBufferInternal::DEPTH_COUNT);
		return 1 << d;
	}

	static inline Depth get_depth_from_size(size_t size) {
		switch (size) {
			case 1:
				return DEPTH_8_BIT;
			case 2:
				return DEPTH_16_BIT;
			case 4:
				return DEPTH_32_BIT;
			case 8:
				return DEPTH_64_BIT;
			default:
				CRASH_NOW();
		}
	}

	static const Depth DEFAULT_CHANNEL_DEPTH = DEPTH_8_BIT;
	static const Depth DEFAULT_TYPE_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_SDF_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_INDICES_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_WEIGHTS_CHANNEL_DEPTH = DEPTH_16_BIT;

	// Limit was made explicit for serialization reasons, and also because there must be a reasonable one
	static const uint32_t MAX_SIZE = 65535;

	struct Channel {
		// Allocated when the channel is populated.
		// Flat array, in order [z][x][y] because it allows faster vertical-wise access (the engine is Y-up).
		uint8_t *data = nullptr;

		// Default value when data is null
		uint64_t defval = 0;

		Depth depth = DEFAULT_CHANNEL_DEPTH;

		uint32_t size_in_bytes = 0;
	};

	VoxelBufferInternal();
	VoxelBufferInternal(VoxelBufferInternal &&src);

	~VoxelBufferInternal();

	VoxelBufferInternal &operator=(VoxelBufferInternal &&src);

	void create(unsigned int sx, unsigned int sy, unsigned int sz);
	void create(Vector3i size);
	void clear();
	void clear_channel(unsigned int channel_index, uint64_t clear_value = 0);
	void clear_channel_f(unsigned int channel_index, real_t clear_value);

	_FORCE_INLINE_ const Vector3i &get_size() const { return _size; }

	void set_default_values(FixedArray<uint64_t, VoxelBufferInternal::MAX_CHANNELS> values);

	uint64_t get_voxel(int x, int y, int z, unsigned int channel_index = 0) const;
	void set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index = 0);

	real_t get_voxel_f(int x, int y, int z, unsigned int channel_index = 0) const;
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index = 0);

	_FORCE_INLINE_ uint64_t get_voxel(const Vector3i pos, unsigned int channel_index = 0) const {
		return get_voxel(pos.x, pos.y, pos.z, channel_index);
	}
	_FORCE_INLINE_ void set_voxel(int value, const Vector3i pos, unsigned int channel_index = 0) {
		set_voxel(value, pos.x, pos.y, pos.z, channel_index);
	}

	void fill(uint64_t defval, unsigned int channel_index = 0);
	void fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index = 0);
	void fill_area_f(float fvalue, Vector3i min, Vector3i max, unsigned int channel_index);
	void fill_f(real_t value, unsigned int channel = 0);

	bool is_uniform(unsigned int channel_index) const;

	void compress_uniform_channels();
	void decompress_channel(unsigned int channel_index);
	Compression get_channel_compression(unsigned int channel_index) const;

	static uint32_t get_size_in_bytes_for_volume(Vector3i size, Depth depth);

	void copy_format(const VoxelBufferInternal &other);

	// Specialized copy functions.
	// Note: these functions don't include metadata on purpose.
	// If you also want to copy metadata, use the specialized functions.
	void copy_from(const VoxelBufferInternal &other);
	void copy_from(const VoxelBufferInternal &other, unsigned int channel_index);
	void copy_from(const VoxelBufferInternal &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min,
			unsigned int channel_index);

	// Copy a region from a box of values, passed as a raw array.
	// `src_size` is the total 3D size of the source box.
	// `src_min` and `src_max` are the sub-region of that box we want to copy.
	// `dst_min` is the lower corner where we want the data to be copied into the destination.
	template <typename T>
	void copy_from(Span<const T> src, Vector3i src_size, Vector3i src_min, Vector3i src_max, Vector3i dst_min,
			unsigned int channel_index) {
		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

		const Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		// Size of source and destination values must match
		ERR_FAIL_COND(channel.depth != get_depth_from_size(sizeof(T)));
#endif

		// This function always decompresses the destination.
		// To keep it compressed, either check what you are about to copy,
		// or schedule a recompression for later.
		decompress_channel(channel_index);

		Span<T> dst(static_cast<T *>(channel.data), channel.size_in_bytes / sizeof(T));
		copy_3d_region_zxy<T>(dst, _size, dst_min, src, src_size, src_min, src_max);
	}

	// Copy a region of the data into a dense buffer.
	// If the source is compressed, it is decompressed.
	// `dst` is a raw array storing grid values in a box.
	// `dst_size` is the total size of the box.
	// `dst_min` is the lower corner of where we want the source data to be stored.
	// `src_min` and `src_max` is the sub-region of the source we want to copy.
	template <typename T>
	void copy_to(Span<T> dst, Vector3i dst_size, Vector3i dst_min, Vector3i src_min, Vector3i src_max,
			unsigned int channel_index) const {
		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

		const Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		// Size of source and destination values must match
		ERR_FAIL_COND(channel.depth != get_depth_from_size(sizeof(T)));
#endif

		if (channel.data == nullptr) {
			fill_3d_region_zxy<T>(dst, dst_size, dst_min, dst_min + (src_max - src_min), channel.defval);
		} else {
			Span<const T> src(static_cast<const T *>(channel.data), channel.size_in_bytes / sizeof(T));
			copy_3d_region_zxy<T>(dst, dst_size, dst_min, src, _size, src_min, src_max);
		}
	}

	// TODO Deprecate?
	// Executes a read-write action on all cells of the provided box that intersect with this buffer.
	// `action_func` receives a voxel value from the channel, and returns a modified value.
	// if the returned value is different, it will be applied to the buffer.
	// Can be used to blend voxels together.
	template <typename F>
	inline void read_write_action(Box3i box, unsigned int channel_index, F action_func) {
		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

		box.clip(Box3i(Vector3i(), _size));
		Vector3i min_pos = box.pos;
		Vector3i max_pos = box.pos + box.size;
		Vector3i pos;
		for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {
				for (pos.y = min_pos.y; pos.y < max_pos.y; ++pos.y) {
					// TODO Optimization: a bunch of checks and branching could be skipped
					const uint64_t v0 = get_voxel(pos, channel_index);
					const uint64_t v1 = action_func(pos, v0);
					if (v0 != v1) {
						set_voxel(v1, pos, channel_index);
					}
				}
			}
		}
	}

	static _FORCE_INLINE_ size_t get_index(const Vector3i pos, const Vector3i size) {
		return pos.get_zxy_index(size);
	}

	_FORCE_INLINE_ size_t get_index(unsigned int x, unsigned int y, unsigned int z) const {
		return y + _size.y * (x + _size.x * z); // ZXY index
	}

	template <typename F>
	inline void for_each_index_and_pos(const Box3i &box, F f) {
		const Vector3i min_pos = box.pos;
		const Vector3i max_pos = box.pos + box.size;
		Vector3i pos;
		for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {
				pos.y = min_pos.y;
				size_t i = get_index(pos.x, pos.y, pos.z);
				for (; pos.y < max_pos.y; ++pos.y) {
					f(i, pos);
					++i;
				}
			}
		}
	}

	// Data_T action_func(Vector3i pos, Data_T in_v)
	template <typename F, typename Data_T>
	void write_box_template(const Box3i &box, unsigned int channel_index, F action_func, Vector3i offset) {
		decompress_channel(channel_index);
		Channel &channel = _channels[channel_index];
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(!Box3i(Vector3i(), _size).contains(box));
		ERR_FAIL_COND(get_depth_byte_count(channel.depth) != sizeof(Data_T));
#endif
		Span<Data_T> data = Span<uint8_t>(channel.data, channel.size_in_bytes)
									.reinterpret_cast_to<Data_T>();
		// `&` is required because lambda captures are `const` by default and `mutable` can be used only from C++23
		for_each_index_and_pos(box, [&data, action_func, offset](size_t i, Vector3i pos) {
			data.set(i, action_func(pos + offset, data[i]));
		});
	}

	// void action_func(Vector3i pos, Data0_T &inout_v0, Data1_T &inout_v1)
	template <typename F, typename Data0_T, typename Data1_T>
	void write_box_2_template(const Box3i &box, unsigned int channel_index0, unsigned channel_index1, F action_func,
			Vector3i offset) {
		decompress_channel(channel_index0);
		decompress_channel(channel_index1);
		Channel &channel0 = _channels[channel_index0];
		Channel &channel1 = _channels[channel_index1];
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND(!Box3i(Vector3i(), _size).contains(box));
		ERR_FAIL_COND(get_depth_byte_count(channel0.depth) != sizeof(Data0_T));
		ERR_FAIL_COND(get_depth_byte_count(channel1.depth) != sizeof(Data1_T));
#endif
		Span<Data0_T> data0 = Span<uint8_t>(channel0.data, channel0.size_in_bytes)
									  .reinterpret_cast_to<Data0_T>();
		Span<Data1_T> data1 = Span<uint8_t>(channel1.data, channel1.size_in_bytes)
									  .reinterpret_cast_to<Data1_T>();
		for_each_index_and_pos(box, [action_func, offset, &data0, &data1](size_t i, Vector3i pos) {
			// TODO The caller must still specify exactly the correct type, maybe some conversion could be used
			action_func(pos + offset, data0[i], data1[i]);
		});
	}

	template <typename F>
	void write_box(const Box3i &box, unsigned int channel_index, F action_func, Vector3i offset) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
#endif
		const Channel &channel = _channels[channel_index];
		switch (channel.depth) {
			case DEPTH_8_BIT:
				write_box_template<F, uint8_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_16_BIT:
				write_box_template<F, uint16_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_32_BIT:
				write_box_template<F, uint32_t>(box, channel_index, action_func, offset);
				break;
			case DEPTH_64_BIT:
				write_box_template<F, uint64_t>(box, channel_index, action_func, offset);
				break;
			default:
				ERR_FAIL();
				break;
		}
	}

	/*template <typename F>
	void write_box_2(const Box3i &box, unsigned int channel_index0, unsigned int channel_index1, F action_func,
			Vector3i offset) {
#ifdef DEBUG_ENABLED
		ERR_FAIL_INDEX(channel_index0, MAX_CHANNELS);
		ERR_FAIL_INDEX(channel_index1, MAX_CHANNELS);
#endif
		const Channel &channel0 = _channels[channel_index0];
		const Channel &channel1 = _channels[channel_index1];
#ifdef DEBUG_ENABLED
		// TODO Find a better way to handle combination explosion. For now I allow only what's really used.
		ERR_FAIL_COND_MSG(channel1.depth != DEPTH_16_BIT, "Second channel depth is hardcoded to 16 for now");
#endif
		switch (channel.depth) {
			case DEPTH_8_BIT:
				write_box_2_template<F, uint8_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_16_BIT:
				write_box_2_template<F, uint16_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_32_BIT:
				write_box_2_template<F, uint32_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			case DEPTH_64_BIT:
				write_box_2_template<F, uint64_t, uint16_t>(box, channel_index0, channel_index1, action_func, offset);
				break;
			default:
				ERR_FAIL();
				break;
		}
	}*/

	static inline FixedArray<uint8_t, MAX_CHANNELS> mask_to_channels_list(
			uint8_t channels_mask, unsigned int &out_count) {
		FixedArray<uint8_t, VoxelBufferInternal::MAX_CHANNELS> channels;
		unsigned int channel_count = 0;

		for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
			if (((1 << channel_index) & channels_mask) != 0) {
				channels[channel_count] = channel_index;
				++channel_count;
			}
		}

		out_count = channel_count;
		return channels;
	}

	void duplicate_to(VoxelBufferInternal &dst, bool include_metadata) const;
	void move_to(VoxelBufferInternal &dst);

	_FORCE_INLINE_ bool is_position_valid(unsigned int x, unsigned int y, unsigned int z) const {
		return x < (unsigned)_size.x && y < (unsigned)_size.y && z < (unsigned)_size.z;
	}

	_FORCE_INLINE_ bool is_position_valid(const Vector3i pos) const {
		return is_position_valid(pos.x, pos.y, pos.z);
	}

	_FORCE_INLINE_ bool is_box_valid(const Box3i box) const {
		return Box3i(Vector3i(), _size).contains(box);
	}

	_FORCE_INLINE_ uint64_t get_volume() const {
		return _size.volume();
	}

	// TODO Have a template version based on channel depth
	bool get_channel_raw(unsigned int channel_index, Span<uint8_t> &slice) const;

	void downscale_to(VoxelBufferInternal &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const;

	bool equals(const VoxelBufferInternal &p_other) const;

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;
	static uint32_t get_depth_bit_count(Depth d);

	// When using lower than 32-bit resolution for terrain signed distance fields,
	// it should be scaled to better fit the range of represented values since the storage is normalized to -1..1.
	// This returns that scale for a given depth configuration.
	static float get_sdf_quantization_scale(Depth d);

	// Metadata

	Variant get_block_metadata() const { return _block_metadata; }
	void set_block_metadata(Variant meta);
	Variant get_voxel_metadata(Vector3i pos) const;
	void set_voxel_metadata(Vector3i pos, Variant meta);

	template <typename F>
	void for_each_voxel_metadata_in_area(Box3i box, F callback) const {
		const Map<Vector3i, Variant>::Element *elem = _voxel_metadata.front();
		while (elem != nullptr) {
			if (box.contains(elem->key())) {
				callback(elem->key(), elem->value());
			}
			elem = elem->next();
		}
	}

	void for_each_voxel_metadata(Ref<FuncRef> callback) const;
	void for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Box3i box) const;

	void clear_voxel_metadata();
	void clear_voxel_metadata_in_area(Box3i box);
	void copy_voxel_metadata_in_area(const VoxelBufferInternal &src_buffer, Box3i src_box, Vector3i dst_origin);
	void copy_voxel_metadata(const VoxelBufferInternal &src_buffer);

	const Map<Vector3i, Variant> &get_voxel_metadata() const { return _voxel_metadata; }

	// Internal synchronization.
	// This lock is optional, and used internally at the moment, only in multithreaded areas.
	inline const RWLock &get_lock() const { return _rw_lock; }
	inline RWLock &get_lock() { return _rw_lock; }

	// Debugging

	Ref<Image> debug_print_sdf_to_image_top_down();

private:
	bool create_channel_noinit(int i, Vector3i size);
	bool create_channel(int i, uint64_t defval);
	void delete_channel(int i);

private:
	// Each channel can store arbitary data.
	// For example, you can decide to store colors (R, G, B, A), gameplay types (type, state, light) or both.
	FixedArray<Channel, MAX_CHANNELS> _channels;

	// How many voxels are there in the three directions. All populated channels have the same size.
	Vector3i _size;

	Variant _block_metadata;
	Map<Vector3i, Variant> _voxel_metadata;

	// TODO It may be preferable to actually move away from storing an RWLock in every buffer in the future.
	// We should be able to find a solution because very few of these locks are actually used at a given time.
	// It worked so far on PC but other platforms like the PS5 might have a pretty low limit (8K?)
	RWLock _rw_lock;
};

inline void debug_check_texture_indices_packed_u16(const VoxelBufferInternal &voxels) {
	for (int z = 0; z < voxels.get_size().z; ++z) {
		for (int x = 0; x < voxels.get_size().x; ++x) {
			for (int y = 0; y < voxels.get_size().y; ++y) {
				uint16_t pi = voxels.get_voxel(x, y, z, VoxelBufferInternal::CHANNEL_INDICES);
				FixedArray<uint8_t, 4> indices = decode_indices_from_packed_u16(pi);
				debug_check_texture_indices(indices);
			}
		}
	}
}

#endif // VOXEL_BUFFER_INTERNAL_H
