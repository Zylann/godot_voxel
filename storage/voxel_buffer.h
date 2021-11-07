#ifndef VOXEL_BUFFER_H
#define VOXEL_BUFFER_H

#include <memory>
#include "voxel_buffer_internal.h"

class VoxelTool;
class Image;
class FuncRef;

// TODO I wish I could call the original class `VoxelBuffer` and expose this other one with that name.
// Godot doesn't seem to allow doing that. So the original class had to be named `VoxelBufferInternal`...

// Scripts-facing wrapper around VoxelBufferInternal.
// It is separate because being a Godot object requires to carry more baggage, and because this data type can
// be instanced many times while being rarely accessed directly from scripts, it is a bit better to take this part out
class VoxelBuffer : public Reference {
	GDCLASS(VoxelBuffer, Reference)

public:
	enum ChannelId {
		CHANNEL_TYPE = VoxelBufferInternal::CHANNEL_TYPE,
		CHANNEL_SDF = VoxelBufferInternal::CHANNEL_SDF,
		CHANNEL_COLOR = VoxelBufferInternal::CHANNEL_COLOR,
		CHANNEL_INDICES = VoxelBufferInternal::CHANNEL_INDICES,
		CHANNEL_WEIGHTS = VoxelBufferInternal::CHANNEL_WEIGHTS,
		CHANNEL_DATA5 = VoxelBufferInternal::CHANNEL_DATA5,
		CHANNEL_DATA6 = VoxelBufferInternal::CHANNEL_DATA6,
		CHANNEL_DATA7 = VoxelBufferInternal::CHANNEL_DATA7,
		MAX_CHANNELS = VoxelBufferInternal::MAX_CHANNELS,
	};

	// TODO use C++17 inline to initialize right here...
	static const char *CHANNEL_ID_HINT_STRING;

	enum Compression {
		COMPRESSION_NONE = VoxelBufferInternal::COMPRESSION_NONE,
		COMPRESSION_UNIFORM = VoxelBufferInternal::COMPRESSION_UNIFORM,
		//COMPRESSION_RLE,
		COMPRESSION_COUNT = VoxelBufferInternal::COMPRESSION_COUNT
	};

	enum Depth {
		DEPTH_8_BIT = VoxelBufferInternal::DEPTH_8_BIT,
		DEPTH_16_BIT = VoxelBufferInternal::DEPTH_16_BIT,
		DEPTH_32_BIT = VoxelBufferInternal::DEPTH_32_BIT,
		DEPTH_64_BIT = VoxelBufferInternal::DEPTH_64_BIT,
		DEPTH_COUNT = VoxelBufferInternal::DEPTH_COUNT
	};

	// Limit was made explicit for serialization reasons, and also because there must be a reasonable one
	static const uint32_t MAX_SIZE = 65535;

	VoxelBuffer();
	VoxelBuffer(std::shared_ptr<VoxelBufferInternal> &other);

	~VoxelBuffer();

	inline const VoxelBufferInternal &get_buffer() const {
#ifdef DEBUG_ENABLED
		CRASH_COND(_buffer == nullptr);
#endif
		return *_buffer;
	}

	inline VoxelBufferInternal &get_buffer() {
#ifdef DEBUG_ENABLED
		CRASH_COND(_buffer == nullptr);
#endif
		return *_buffer;
	}

	//inline std::shared_ptr<VoxelBufferInternal> get_buffer_shared() { return _buffer; }

	Vector3 get_size() const {
		return _buffer->get_size().to_vec3();
	}
	int get_size_x() const { return _buffer->get_size().x; }
	int get_size_y() const { return _buffer->get_size().x; }
	int get_size_z() const { return _buffer->get_size().x; }

	void create(int x, int y, int z) {
		_buffer->create(x, y, z);
	}
	void clear();

	uint64_t get_voxel(int x, int y, int z, unsigned int channel) const {
		return _buffer->get_voxel(x, y, z, channel);
	}
	void set_voxel(uint64_t value, int x, int y, int z, unsigned int channel) {
		_buffer->set_voxel(value, x, y, z, channel);
	}
	real_t get_voxel_f(int x, int y, int z, unsigned int channel_index) const;
	void set_voxel_f(real_t value, int x, int y, int z, unsigned int channel_index);

	void set_voxel_v(uint64_t value, Vector3 pos, unsigned int channel_index) {
		_buffer->set_voxel(value, pos.x, pos.y, pos.z, channel_index);
	}

	void copy_channel_from(Ref<VoxelBuffer> other, unsigned int channel);
	void copy_channel_from_area(
			Ref<VoxelBuffer> other, Vector3 src_min, Vector3 src_max, Vector3 dst_min, unsigned int channel);

	void fill(uint64_t defval, unsigned int channel_index = 0);
	void fill_f(real_t value, unsigned int channel = 0);
	void fill_area(uint64_t defval, Vector3 min, Vector3 max, unsigned int channel_index) {
		_buffer->fill_area(defval, Vector3i::from_floored(min), Vector3i::from_floored(max), channel_index);
	}

	bool is_uniform(unsigned int channel_index) const;

	void compress_uniform_channels();
	Compression get_channel_compression(unsigned int channel_index) const;

	void downscale_to(Ref<VoxelBuffer> dst, Vector3 src_min, Vector3 src_max, Vector3 dst_min) const;

	Ref<VoxelBuffer> duplicate(bool include_metadata) const;

	Ref<VoxelTool> get_voxel_tool();

	void set_channel_depth(unsigned int channel_index, Depth new_depth);
	Depth get_channel_depth(unsigned int channel_index) const;

	// When using lower than 32-bit resolution for terrain signed distance fields,
	// it should be scaled to better fit the range of represented values since the storage is normalized to -1..1.
	// This returns that scale for a given depth configuration.
	static float get_sdf_quantization_scale(Depth d);

	// Metadata

	Variant get_block_metadata() const { return _buffer->get_block_metadata(); }
	void set_block_metadata(Variant meta);

	Variant get_voxel_metadata(Vector3 pos) const {
		return _buffer->get_voxel_metadata(Vector3i::from_floored(pos));
	}
	void set_voxel_metadata(Vector3 pos, Variant meta) {
		_buffer->set_voxel_metadata(Vector3i::from_floored(pos), meta);
	}
	void for_each_voxel_metadata(Ref<FuncRef> callback) const;
	void for_each_voxel_metadata_in_area(Ref<FuncRef> callback, Vector3 min_pos, Vector3 max_pos);
	void copy_voxel_metadata_in_area(
			Ref<VoxelBuffer> src_buffer, Vector3 src_min_pos, Vector3 src_max_pos, Vector3 dst_pos);

	void clear_voxel_metadata();
	void clear_voxel_metadata_in_area(Vector3 min_pos, Vector3 max_pos);

	// Debugging

	Ref<Image> debug_print_sdf_to_image_top_down();

private:
	static void _bind_methods();

	// Not sure yet if we'll really need shared_ptr or just no pointer
	std::shared_ptr<VoxelBufferInternal> _buffer;
};

VARIANT_ENUM_CAST(VoxelBuffer::ChannelId)
VARIANT_ENUM_CAST(VoxelBuffer::Depth)
VARIANT_ENUM_CAST(VoxelBuffer::Compression)

#endif // VOXEL_BUFFER_H
