#ifndef VOXEL_BUFFER_H
#define VOXEL_BUFFER_H

#include "math/rect3i.h"
#include "util/array_slice.h"
#include "util/fixed_array.h"

#include <core/reference.h>
#include <core/vector.h>

class VoxelTool;
class Image;

// Dense voxels data storage.
// Organized in channels of configurable bit depth.
// Values can be interpreted either as unsigned integers or normalized floats.
class VoxelBuffer : public Reference {
	GDCLASS(VoxelBuffer, Reference)

public:
	enum ChannelId {
		CHANNEL_TYPE = 0,
		CHANNEL_SDF,
		CHANNEL_DATA2,
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

	VoxelBuffer();
	~VoxelBuffer();

	void create(int sx, int sy, int sz);
	void create(Vector3i size);
	void clear();
	void clear_channel(unsigned int channel_index, uint64_t clear_value = 0);
	void clear_channel_f(unsigned int channel_index, real_t clear_value);

	_FORCE_INLINE_ const Vector3i &get_size() const { return _size; }

	void set_default_values(FixedArray<uint64_t, VoxelBuffer::MAX_CHANNELS> values);

	uint64_t get_voxel(int x, int y, int z, unsigned int channel_index = 0) const;
	void set_voxel(uint64_t value, int x, int y, int z, unsigned int channel_index = 0);

	void try_set_voxel(int x, int y, int z, int value, unsigned int channel_index = 0);

	real_t get_voxel_f(int x, int y, int z, unsigned int channel_index = 0) const;
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index = 0);

	_FORCE_INLINE_ uint64_t get_voxel(const Vector3i pos, unsigned int channel_index = 0) const { return get_voxel(pos.x, pos.y, pos.z, channel_index); }
	_FORCE_INLINE_ void set_voxel(int value, const Vector3i pos, unsigned int channel_index = 0) { set_voxel(value, pos.x, pos.y, pos.z, channel_index); }

	void fill(uint64_t defval, unsigned int channel_index = 0);
	void fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index = 0);
	void fill_f(real_t value, unsigned int channel = 0);

	bool is_uniform(unsigned int channel_index) const;

	void compress_uniform_channels();
	void decompress_channel(unsigned int channel_index);
	Compression get_channel_compression(unsigned int channel_index) const;

	static uint32_t get_size_in_bytes_for_volume(Vector3i size, Depth depth);

	void copy_from(const VoxelBuffer &other);
	void copy_from(const VoxelBuffer &other, unsigned int channel_index);
	void copy_from(const VoxelBuffer &other, Vector3i src_min, Vector3i src_max, Vector3i dst_min, unsigned int channel_index);

	Ref<VoxelBuffer> duplicate() const;

	_FORCE_INLINE_ bool validate_pos(unsigned int x, unsigned int y, unsigned int z) const {
		return x < (unsigned)_size.x && y < (unsigned)_size.y && z < (unsigned)_size.z;
	}

	_FORCE_INLINE_ unsigned int index(unsigned int x, unsigned int y, unsigned int z) const {
		return y + _size.y * (x + _size.x * z);
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

	bool equals(const VoxelBuffer *p_other) const;

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;
	static uint32_t get_depth_bit_count(Depth d);

	// TODO Make this work, would be awesome for perf
	//
	//	template <typename F>
	//	void read_write_action(Rect3i box, Vector3i offset, unsigned int channel_index, F f) {
	//		ERR_FAIL_INDEX(channel_index, MAX_CHANNELS);
	//		box.clip(Rect3i(Vector3i(), _size));
	//		Vector3i min_pos = box.pos;
	//		Vector3i max_pos = box.pos + box.size;
	//		Vector3i pos;
	//		for (pos.z = min_pos.z; pos.z < max_pos.z; ++pos.z) {
	//			for (pos.x = min_pos.x; pos.x < max_pos.x; ++pos.x) {
	//				for (pos.y = min_pos.y; pos.y < max_pos.y; ++pos.y) {
	//					int v0 = get_voxel(pos, channel_index);
	//					int v1 = f(pos + offset, v0);
	//					if (v0 != v1) {
	//						set_voxel(v1, pos, channel_index);
	//					}
	//				}
	//			}
	//		}
	//	}

	// Debugging
	Ref<Image> debug_print_sdf_to_image_top_down();

private:
	void create_channel_noinit(int i, Vector3i size);
	void create_channel(int i, Vector3i size, uint64_t defval);
	void delete_channel(int i);

protected:
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
};

VARIANT_ENUM_CAST(VoxelBuffer::ChannelId)
VARIANT_ENUM_CAST(VoxelBuffer::Depth)

#endif // VOXEL_BUFFER_H
