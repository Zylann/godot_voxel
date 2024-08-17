#ifndef VOXEL_BUFFER_GD_H
#define VOXEL_BUFFER_GD_H

#include "../util/godot/classes/ref_counted.h"
#include "../util/godot/core/array.h"
#include "../util/godot/core/typed_array.h"
#include "../util/macros.h"
#include "../util/math/vector3i.h"
#include "voxel_buffer.h"
#include <cstdint>
#include <memory>

ZN_GODOT_FORWARD_DECLARE(class Image)

namespace zylann::voxel {

class VoxelTool;

namespace godot {

// Scripts-facing wrapper around VoxelBuffer.
// It is separate because being a Godot object requires to carry more baggage, and because this data type can
// be instanced many times while being rarely accessed directly from scripts, it is a bit better to take this part out
class VoxelBuffer : public RefCounted {
	GDCLASS(VoxelBuffer, RefCounted)

public:
	enum ChannelId {
		CHANNEL_TYPE = zylann::voxel::VoxelBuffer::CHANNEL_TYPE,
		CHANNEL_SDF = zylann::voxel::VoxelBuffer::CHANNEL_SDF,
		CHANNEL_COLOR = zylann::voxel::VoxelBuffer::CHANNEL_COLOR,
		CHANNEL_INDICES = zylann::voxel::VoxelBuffer::CHANNEL_INDICES,
		CHANNEL_WEIGHTS = zylann::voxel::VoxelBuffer::CHANNEL_WEIGHTS,
		CHANNEL_DATA5 = zylann::voxel::VoxelBuffer::CHANNEL_DATA5,
		CHANNEL_DATA6 = zylann::voxel::VoxelBuffer::CHANNEL_DATA6,
		CHANNEL_DATA7 = zylann::voxel::VoxelBuffer::CHANNEL_DATA7,
		MAX_CHANNELS = zylann::voxel::VoxelBuffer::MAX_CHANNELS,
	};

	// TODO use C++17 inline to initialize right here...
	static const char *CHANNEL_ID_HINT_STRING;

	enum Compression {
		COMPRESSION_NONE = zylann::voxel::VoxelBuffer::COMPRESSION_NONE,
		COMPRESSION_UNIFORM = zylann::voxel::VoxelBuffer::COMPRESSION_UNIFORM,
		// COMPRESSION_RLE,
		COMPRESSION_COUNT = zylann::voxel::VoxelBuffer::COMPRESSION_COUNT
	};

	enum Depth {
		DEPTH_8_BIT = zylann::voxel::VoxelBuffer::DEPTH_8_BIT,
		DEPTH_16_BIT = zylann::voxel::VoxelBuffer::DEPTH_16_BIT,
		DEPTH_32_BIT = zylann::voxel::VoxelBuffer::DEPTH_32_BIT,
		DEPTH_64_BIT = zylann::voxel::VoxelBuffer::DEPTH_64_BIT,
		DEPTH_COUNT = zylann::voxel::VoxelBuffer::DEPTH_COUNT
	};

	enum Allocator {
		ALLOCATOR_DEFAULT = zylann::voxel::VoxelBuffer::ALLOCATOR_DEFAULT,
		ALLOCATOR_POOL = zylann::voxel::VoxelBuffer::ALLOCATOR_POOL,
		ALLOCATOR_COUNT
	};

	// Limit was made explicit for serialization reasons, and also because there must be a reasonable one
	static const uint32_t MAX_SIZE = 65535;

	// Constructs a new buffer
	VoxelBuffer();
	VoxelBuffer(VoxelBuffer::Allocator allocator);
	// Reference an existing buffer
	VoxelBuffer(std::shared_ptr<zylann::voxel::VoxelBuffer> &other);

	~VoxelBuffer();

	// Workaround because the constructor with arguments cannot always be used due to Godot limitations
	static Ref<VoxelBuffer> create_shared(std::shared_ptr<zylann::voxel::VoxelBuffer> &other);

	inline const zylann::voxel::VoxelBuffer &get_buffer() const {
#ifdef DEBUG_ENABLED
		CRASH_COND(_buffer == nullptr);
#endif
		return *_buffer;
	}

	inline zylann::voxel::VoxelBuffer &get_buffer() {
#ifdef DEBUG_ENABLED
		CRASH_COND(_buffer == nullptr);
#endif
		return *_buffer;
	}

	inline std::shared_ptr<zylann::voxel::VoxelBuffer> get_buffer_shared() {
#ifdef DEBUG_ENABLED
		CRASH_COND(_buffer == nullptr);
#endif
		return _buffer;
	}

	// inline std::shared_ptr<zylann::voxel::VoxelBuffer> get_buffer_shared() { return _buffer; }

	Vector3i get_size() const {
		return _buffer->get_size();
	}

	void create(int x, int y, int z);
	void clear();

	uint64_t get_voxel(int x, int y, int z, unsigned int channel) const {
		return _buffer->get_voxel(x, y, z, channel);
	}
	void set_voxel(uint64_t value, int x, int y, int z, unsigned int channel) {
		_buffer->set_voxel(value, x, y, z, channel);
	}
	real_t get_voxel_f(int x, int y, int z, unsigned int channel_index) const;
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index);

	void set_voxel_v(uint64_t value, Vector3i pos, unsigned int channel_index) {
		_buffer->set_voxel(value, pos.x, pos.y, pos.z, channel_index);
	}

	void copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel);
	void copy_channel_from_area(
			Ref<VoxelBuffer> other,
			Vector3i src_min,
			Vector3i src_max,
			Vector3i dst_min,
			unsigned int channel
	);

	void fill(uint64_t defval, int channel_index = 0);
	void fill_f(real_t value, int channel = 0);
	void fill_area(uint64_t defval, Vector3i min, Vector3i max, unsigned int channel_index) {
		_buffer->fill_area(defval, min, max, channel_index);
	}
	void fill_area_f(real_t value, Vector3i min, Vector3i max, unsigned int channel_index) {
		_buffer->fill_area_f(value, min, max, channel_index);
	}

	bool is_uniform(int channel_index) const;

	void compress_uniform_channels();
	Compression get_channel_compression(int channel_index) const;
	void decompress_channel(int channel_index);

	void downscale_to(Ref<VoxelBuffer> dst, Vector3i src_min, Vector3i src_max, Vector3i dst_min) const;

	Ref<VoxelBuffer> duplicate(bool include_metadata) const;

	Ref<VoxelTool> get_voxel_tool();

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;

	void remap_values(unsigned int channel_index, PackedInt32Array map);

	// When using lower than 32-bit resolution for terrain signed distance fields,
	// it should be scaled to better fit the range of represented values since the storage is normalized to -1..1.
	// This returns that scale for a given depth configuration.
	static float get_sdf_quantization_scale(Depth d);

	Allocator get_allocator() const;

	// Operations

	void op_add_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel);
	void op_sub_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel);
	void op_mul_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel);
	void op_mul_value_f(float scale, VoxelBuffer::ChannelId channel);
	void op_min_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel);
	void op_max_buffer_f(Ref<VoxelBuffer> other, VoxelBuffer::ChannelId channel);

	// Checks if float/SDF values from a channel of the source buffer are lower than a threshold, and sets an integer
	// value into the destination buffer depending on the result of that comparison.
	void op_select_less_src_f_dst_i_values(
			Ref<VoxelBuffer> src_ref,
			const VoxelBuffer::ChannelId src_channel,
			const float threshold,
			const int value_if_less,
			const int value_if_more,
			const VoxelBuffer::ChannelId dst_channel
	);

	// Metadata

	Variant get_block_metadata() const;
	void set_block_metadata(Variant meta);

	Variant get_voxel_metadata(Vector3i pos) const;
	void set_voxel_metadata(Vector3i pos, Variant meta);

	void for_each_voxel_metadata(const Callable &callback) const;
	void for_each_voxel_metadata_in_area(const Callable &callback, Vector3i min_pos, Vector3i max_pos);
	void copy_voxel_metadata_in_area(
			Ref<VoxelBuffer> src_buffer,
			Vector3i src_min_pos,
			Vector3i src_max_pos,
			Vector3i dst_pos
	);

	void clear_voxel_metadata();
	void clear_voxel_metadata_in_area(Vector3i min_pos, Vector3i max_pos);

	// Debugging

	Ref<Image> debug_print_sdf_to_image_top_down();
	static Ref<Image> debug_print_sdf_to_image_top_down(const zylann::voxel::VoxelBuffer &vb);
	TypedArray<Image> debug_print_sdf_y_slices(float scale) const;
	Ref<Image> debug_print_sdf_y_slice(float scale, int y) const;
	static Ref<Image> debug_print_sdf_y_slice(const zylann::voxel::VoxelBuffer &buffer, float scale, int y);
	static Ref<Image> debug_print_sdf_z_slice(const zylann::voxel::VoxelBuffer &buffer, float scale, int z);

private:
	// In GDExtension, `create` is defined by `GDCLASS`, preventing anyone from binding a `create` function directly
	void _b_create(int x, int y, int z) {
		create(x, y, z);
	}

	static void _bind_methods();

	std::shared_ptr<zylann::voxel::VoxelBuffer> _buffer;
};

} // namespace godot
} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::godot::VoxelBuffer::ChannelId)
VARIANT_ENUM_CAST(zylann::voxel::godot::VoxelBuffer::Depth)
VARIANT_ENUM_CAST(zylann::voxel::godot::VoxelBuffer::Compression)
VARIANT_ENUM_CAST(zylann::voxel::godot::VoxelBuffer::Allocator)

#endif // VOXEL_BUFFER_GD_H
