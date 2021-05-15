#ifndef VOXEL_BUFFER_H
#define VOXEL_BUFFER_H

#include "../constants/voxel_constants.h"
#include "../util/array_slice.h"
#include "../util/fixed_array.h"
#include "../util/math/rect3i.h"

#include <core/map.h>
#include <core/reference.h>
#include <core/vector.h>

class VoxelTool;
class Image;
class FuncRef;

// Dense voxels data storage.
// Organized in channels of configurable bit depth.
// Values can be interpreted either as unsigned integers or normalized floats.
class VoxelBuffer : public Reference {
	GDCLASS(VoxelBuffer, Reference)

public:
	enum ChannelId {
		CHANNEL_TYPE = 0,
		CHANNEL_SDF,
		CHANNEL_COLOR,
		CHANNEL_DATA3,
		CHANNEL_DATA4,
		CHANNEL_DATA5,
		CHANNEL_DATA6,
		CHANNEL_DATA7,
		// Arbitrary value, 8 should be enough. Tweak for your needs.
		MAX_CHANNELS
	};

	// TODO use C++17 inline to initialize right here...
	static const char *CHANNEL_ID_HINT_STRING;

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

	static const Depth DEFAULT_CHANNEL_DEPTH = DEPTH_8_BIT;
	static const Depth DEFAULT_TYPE_CHANNEL_DEPTH = DEPTH_16_BIT;
	static const Depth DEFAULT_SDF_CHANNEL_DEPTH = DEPTH_16_BIT;

	// Limit was made explicit for serialization reasons, and also because there must be a reasonable one
	static const uint32_t MAX_SIZE = 65535;

	VoxelBuffer();
	~VoxelBuffer();

	void create(unsigned int sx, unsigned int sy, unsigned int sz);
	void create(Vector3i size);
	void clear();
	void clear_channel(unsigned int channel_index, uint64_t clear_value = 0);
	void clear_channel_f(unsigned int channel_index, real_t clear_value);

	_FORCE_INLINE_ const Vector3i &get_size() const { return _size; }

	void set_default_values(FixedArray<uint64_t, VoxelBuffer::MAX_CHANNELS> values);

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

	void copy_format(const VoxelBuffer &other);

	// Specialized copy functions.
	// Note: these functions don't include metadata on purpose.
	// If you also want to copy metadata, use the specialized functions.
	void copy_from(const VoxelBuffer &other);
	void copy_from(const VoxelBuffer &other, unsigned int channel_index);
	void copy_from(const VoxelBuffer &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min,
			unsigned int channel_index);

	// Executes a read-write action on all cells of the provided box that intersect with this buffer.
	// `action_func` receives a voxel value from the channel, and returns a modified value.
	// if the returned value is different, it will be applied to the buffer.
	// Can be used to blend voxels together.
	template <typename F>
	inline void read_write_action(Rect3i box, unsigned int channel_index, F action_func) {
		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);

		box.clip(Rect3i(Vector3i(), _size));
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

	static inline FixedArray<uint8_t, MAX_CHANNELS> mask_to_channels_list(
			uint8_t channels_mask, unsigned int &out_count) {

		FixedArray<uint8_t, VoxelBuffer::MAX_CHANNELS> channels;
		unsigned int channel_count = 0;

		for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {
			if (((1 << channel_index) & channels_mask) != 0) {
				channels[channel_count] = channel_index;
				++channel_count;
			}
		}

		out_count = channel_count;
		return channels;
	}

	Ref<VoxelBuffer> duplicate(bool include_metadata) const;

	_FORCE_INLINE_ bool is_position_valid(unsigned int x, unsigned int y, unsigned int z) const {
		return x < (unsigned)_size.x && y < (unsigned)_size.y && z < (unsigned)_size.z;
	}

	_FORCE_INLINE_ bool is_position_valid(const Vector3i pos) const {
		return is_position_valid(pos.x, pos.y, pos.z);
	}

	_FORCE_INLINE_ bool is_box_valid(const Rect3i box) const {
		return Rect3i(Vector3i(), _size).contains(box);
	}

	static _FORCE_INLINE_ unsigned int get_index(const Vector3i pos, const Vector3i size) {
		return pos.get_zxy_index(size);
	}

	_FORCE_INLINE_ unsigned int get_index(unsigned int x, unsigned int y, unsigned int z) const {
		return y + _size.y * (x + _size.x * z); // ZXY index
	}

	//	_FORCE_INLINE_ unsigned int row_index(unsigned int x, unsigned int y, unsigned int z) const {
	//		return _size.y * (x + _size.x * z);
	//	}

	_FORCE_INLINE_ unsigned int get_volume() const {
		return _size.x * _size.y * _size.z;
	}

	// TODO Have a template version based on channel depth
	bool get_channel_raw(unsigned int channel_index, ArraySlice<uint8_t> &slice) const;

	void downscale_to(VoxelBuffer &dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const;
	Ref<VoxelTool> get_voxel_tool();

	bool equals(const VoxelBuffer &p_other) const;

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;
	static uint32_t get_depth_bit_count(Depth d);

	// When using lower than 32-bit resolution for terrain signed distance fields,
	// it should be scaled to better fit the range of represented values since the storage is normalized to -1..1.
	// This returns that scale for a given depth configuration.
	static float get_sdf_quantization_scale(Depth d);

	// TODO Switch to using GPU format inorm16 for these conversions
	// The current ones seem to work but aren't really correct

	static inline float u8_to_norm(uint8_t v) {
		return (static_cast<float>(v) - 0x7f) * VoxelConstants::INV_0x7f;
	}

	static inline float u16_to_norm(uint16_t v) {
		return (static_cast<float>(v) - 0x7fff) * VoxelConstants::INV_0x7fff;
	}

	static inline uint8_t norm_to_u8(float v) {
		return clamp(static_cast<int>(128.f * v + 128.f), 0, 0xff);
	}

	static inline uint16_t norm_to_u16(float v) {
		return clamp(static_cast<int>(0x8000 * v + 0x8000), 0, 0xffff);
	}

	/*static inline float quantized_u8_to_real(uint8_t v) {
		return u8_to_norm(v) * VoxelConstants::QUANTIZED_SDF_8_BITS_SCALE_INV;
	}

	static inline float quantized_u16_to_real(uint8_t v) {
		return u8_to_norm(v) * VoxelConstants::QUANTIZED_SDF_16_BITS_SCALE_INV;
	}

	static inline uint8_t real_to_quantized_u8(float v) {
		return norm_to_u8(v * VoxelConstants::QUANTIZED_SDF_8_BITS_SCALE);
	}

	static inline uint16_t real_to_quantized_u16(float v) {
		return norm_to_u16(v * VoxelConstants::QUANTIZED_SDF_16_BITS_SCALE);
	}*/

	// Metadata

	Variant get_block_metadata() const { return _block_metadata; }
	void set_block_metadata(Variant meta);
	Variant get_voxel_metadata(Vector3i pos) const;
	void set_voxel_metadata(Vector3i pos, Variant meta);
	void for_each_voxel_metadata(Ref<FuncRef> callback) const;
	void for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Rect3i box) const;
	void clear_voxel_metadata();
	void clear_voxel_metadata_in_area(Rect3i box);
	void copy_voxel_metadata_in_area(Ref<VoxelBuffer> src_buffer, Rect3i src_box, Vector3i dst_origin);
	void copy_voxel_metadata(const VoxelBuffer &src_buffer);

	const Map<Vector3i, Variant> &get_voxel_metadata() const { return _voxel_metadata; }

	// Internal synchronization.
	// This lock is optional, and used internally at the moment, only in multithreaded areas.
	inline const RWLock &get_lock() const { return _rw_lock; }
	inline RWLock &get_lock() { return _rw_lock; }

	// Debugging

	Ref<Image> debug_print_sdf_to_image_top_down();

private:
	void create_channel_noinit(int i, Vector3i size);
	void create_channel(int i, Vector3i size, uint64_t defval);
	void delete_channel(int i);

	static void _bind_methods();

	int get_size_x() const { return _size.x; }
	int get_size_y() const { return _size.y; }
	int get_size_z() const { return _size.z; }

	// Bindings
	Vector3 _b_get_size() const { return _size.to_vec3(); }
	void _b_create(int x, int y, int z) { create(x, y, z); }
	uint64_t _b_get_voxel(int x, int y, int z, unsigned int channel) const { return get_voxel(x, y, z, channel); }
	void _b_set_voxel(uint64_t value, int x, int y, int z, unsigned int channel) { set_voxel(value, x, y, z, channel); }
	void _b_copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel);
	void _b_copy_channel_from_area(Ref<VoxelBuffer> other, Vector3 src_min, Vector3 src_max, Vector3 dst_min, unsigned int channel);
	void _b_fill_area(uint64_t defval, Vector3 min, Vector3 max, unsigned int channel_index) { fill_area(defval, Vector3i(min), Vector3i(max), channel_index); }
	void _b_set_voxel_f(real_t value, int x, int y, int z, unsigned int channel) { set_voxel_f(value, x, y, z, channel); }
	void _b_set_voxel_v(uint64_t value, Vector3 pos, unsigned int channel_index = 0) { set_voxel(value, pos.x, pos.y, pos.z, channel_index); }
	void _b_downscale_to(Ref<VoxelBuffer> dst, Vector3 src_min, Vector3 src_max, Vector3 dst_min) const;
	Variant _b_get_voxel_metadata(Vector3 pos) const { return get_voxel_metadata(Vector3i(pos)); }
	void _b_set_voxel_metadata(Vector3 pos, Variant meta) { set_voxel_metadata(Vector3i(pos), meta); }
	void _b_for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Vector3 min_pos, Vector3 max_pos);
	void _b_clear_voxel_metadata_in_area(Vector3 min_pos, Vector3 max_pos);
	void _b_copy_voxel_metadata_in_area(Ref<VoxelBuffer> src_buffer, Vector3 src_min_pos, Vector3 src_max_pos, Vector3 dst_pos);

private:
	struct Channel {
		// Allocated when the channel is populated.
		// Flat array, in order [z][x][y] because it allows faster vertical-wise access (the engine is Y-up).
		uint8_t *data = nullptr;

		// Default value when data is null
		uint64_t defval = 0;

		Depth depth = DEFAULT_CHANNEL_DEPTH;

		uint32_t size_in_bytes = 0;
	};

	// Each channel can store arbitary data.
	// For example, you can decide to store colors (R, G, B, A), gameplay types (type, state, light) or both.
	FixedArray<Channel, MAX_CHANNELS> _channels;

	// How many voxels are there in the three directions. All populated channels have the same size.
	Vector3i _size;

	Variant _block_metadata;
	Map<Vector3i, Variant> _voxel_metadata;

	RWLock _rw_lock;
};

VARIANT_ENUM_CAST(VoxelBuffer::ChannelId)
VARIANT_ENUM_CAST(VoxelBuffer::Depth)
VARIANT_ENUM_CAST(VoxelBuffer::Compression)

#endif // VOXEL_BUFFER_H
