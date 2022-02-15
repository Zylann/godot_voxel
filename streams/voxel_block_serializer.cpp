#include "voxel_block_serializer.h"
#include "../storage/voxel_buffer_internal.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/macros.h"
#include "../util/math/vector3i.h"
#include "../util/profiling.h"
#include "compressed_data.h"

#include <core/io/marshalls.h>
#include <core/io/stream_peer.h>
//#include <core/map.h>
#include <core/io/file_access.h>
#include <limits>

namespace zylann::voxel {
namespace BlockSerializer {

const uint8_t BLOCK_FORMAT_VERSION = 2;
const unsigned int BLOCK_TRAILING_MAGIC = 0x900df00d;
const unsigned int BLOCK_TRAILING_MAGIC_SIZE = 4;
const unsigned int BLOCK_METADATA_HEADER_SIZE = sizeof(uint32_t);

// Temporary data buffers, re-used to reduce allocations
thread_local std::vector<uint8_t> tls_data;
thread_local std::vector<uint8_t> tls_compressed_data;
thread_local std::vector<uint8_t> tls_metadata_tmp;

size_t get_metadata_size_in_bytes(const VoxelBufferInternal &buffer) {
	size_t size = 0;

	const FlatMap<Vector3i, Variant> &voxel_metadata = buffer.get_voxel_metadata();
	for (FlatMap<Vector3i, Variant>::ConstIterator it = voxel_metadata.begin(); it != voxel_metadata.end(); ++it) {
		const Vector3i pos = it->key;

		ERR_FAIL_COND_V_MSG(pos.x < 0 || static_cast<uint32_t>(pos.x) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata X position");
		ERR_FAIL_COND_V_MSG(pos.y < 0 || static_cast<uint32_t>(pos.y) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata Y position");
		ERR_FAIL_COND_V_MSG(pos.z < 0 || static_cast<uint32_t>(pos.z) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata Z position");

		size += 3 * sizeof(uint16_t); // Positions are stored as 3 unsigned shorts

		int len;
		const Error err = encode_variant(it->value, nullptr, len, false);
		ERR_FAIL_COND_V_MSG(err != OK, 0, "Error when trying to encode voxel metadata.");
		size += len;
	}

	// If no metadata is found at all, nothing is serialized, not even null.
	// It spares 24 bytes (40 if real_t == double),
	// and is backward compatible with saves made before introduction of metadata.

	if (size != 0 || buffer.get_block_metadata() != Variant()) {
		int len;
		// Get size first by invoking the function is "length mode"
		const Error err = encode_variant(buffer.get_block_metadata(), nullptr, len, false);
		ERR_FAIL_COND_V_MSG(err != OK, 0, "Error when trying to encode block metadata.");
		size += len;
	}

	return size;
}

template <typename T>
inline void write(uint8_t *&dst, T d) {
	*(T *)dst = d;
	dst += sizeof(T);
}

template <typename T>
inline T read(uint8_t *&src) {
	T d = *(T *)src;
	src += sizeof(T);
	return d;
}

// The target buffer MUST have correct size. Recoverable errors must have been checked before.
void serialize_metadata(uint8_t *p_dst, const VoxelBufferInternal &buffer, const size_t metadata_size) {
	uint8_t *dst = p_dst;

	{
		int written_length;
		encode_variant(buffer.get_block_metadata(), dst, written_length, false);
		dst += written_length;

		// I chose to cast this way to fix a GCC warning.
		// If dst - p_dst is negative (which is wrong), it will wrap and cause a justified assertion failure
		CRASH_COND_MSG(static_cast<size_t>(dst - p_dst) > metadata_size, "Wrote block metadata out of expected bounds");
	}

	const FlatMap<Vector3i, Variant> &voxel_metadata = buffer.get_voxel_metadata();
	for (FlatMap<Vector3i, Variant>::ConstIterator it = voxel_metadata.begin(); it != voxel_metadata.end(); ++it) {
		// Serializing key as ushort because it's more than enough for a 3D dense array
		static_assert(VoxelBufferInternal::MAX_SIZE <= std::numeric_limits<uint16_t>::max(),
				"Maximum size exceeds serialization support");
		const Vector3i pos = it->key;
		write<uint16_t>(dst, pos.x);
		write<uint16_t>(dst, pos.y);
		write<uint16_t>(dst, pos.z);

		int written_length;
		const Error err = encode_variant(it->value, dst, written_length, false);
		CRASH_COND_MSG(err != OK, "Error when trying to encode voxel metadata.");
		dst += written_length;

		CRASH_COND_MSG(static_cast<size_t>(dst - p_dst) > metadata_size, "Wrote voxel metadata out of expected bounds");
	}

	CRASH_COND_MSG(static_cast<size_t>(dst - p_dst) != metadata_size,
			String("Written metadata doesn't match expected count (expected {0}, got {1})")
					.format(varray(SIZE_T_TO_VARIANT(metadata_size), (int)(dst - p_dst))));
}

template <typename T>
struct ClearOnExit {
	T &container;
	~ClearOnExit() {
		container.clear();
	}
};

//#define CLEAR_ON_EXIT(container) ClearOnExit<decltype(container)> clear_on_exit_##__LINE__;

bool deserialize_metadata(uint8_t *p_src, VoxelBufferInternal &buffer, const size_t metadata_size) {
	uint8_t *src = p_src;
	size_t remaining_length = metadata_size;

	{
		Variant block_metadata;
		int read_length;
		const Error err = decode_variant(block_metadata, src, remaining_length, &read_length, false);
		ERR_FAIL_COND_V_MSG(err != OK, false, "Failed to deserialize block metadata");
		remaining_length -= read_length;
		src += read_length;
		CRASH_COND_MSG(remaining_length > metadata_size, "Block metadata size underflow");
		buffer.set_block_metadata(block_metadata);
	}

	typedef FlatMap<Vector3i, Variant>::Pair Pair;
	static thread_local std::vector<Pair> tls_pairs;
	// Clear when exiting scope (including cases of error) so we don't store dangling Variants
	ClearOnExit<std::vector<Pair>> clear_tls_pairs{ tls_pairs };

	while (remaining_length > 0) {
		Vector3i pos;
		pos.x = read<uint16_t>(src);
		pos.y = read<uint16_t>(src);
		pos.z = read<uint16_t>(src);
		remaining_length -= 3 * sizeof(uint16_t);

		ERR_CONTINUE_MSG(!buffer.is_position_valid(pos),
				String("Invalid voxel metadata position {0} for buffer of size {1}")
						.format(varray(pos, buffer.get_size())));

		Variant metadata;
		int read_length;
		const Error err = decode_variant(metadata, src, remaining_length, &read_length, false);
		ERR_FAIL_COND_V_MSG(err != OK, false, "Failed to deserialize block metadata");
		remaining_length -= read_length;
		src += read_length;
		CRASH_COND_MSG(remaining_length > metadata_size, "Block metadata size underflow");

		tls_pairs.push_back(Pair{ pos, metadata });
	}

	// Set all metadata at once, FlatMap is faster to initialize this way
	buffer.clear_and_set_voxel_metadata(to_span(tls_pairs));

	CRASH_COND_MSG(remaining_length != 0, "Did not read expected size");
	return true;
}

size_t get_size_in_bytes(const VoxelBufferInternal &buffer, size_t &metadata_size) {
	// Version and size
	size_t size = 1 * sizeof(uint8_t) + 3 * sizeof(uint16_t);

	const Vector3i size_in_voxels = buffer.get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		const VoxelBufferInternal::Compression compression = buffer.get_channel_compression(channel_index);
		const VoxelBufferInternal::Depth depth = buffer.get_channel_depth(channel_index);

		// For format value
		size += 1;

		switch (compression) {
			case VoxelBufferInternal::COMPRESSION_NONE: {
				size += VoxelBufferInternal::get_size_in_bytes_for_volume(size_in_voxels, depth);
			} break;

			case VoxelBufferInternal::COMPRESSION_UNIFORM: {
				size += VoxelBufferInternal::get_depth_bit_count(depth) >> 3;
			} break;

			default:
				ERR_PRINT("Unhandled compression mode");
				CRASH_NOW();
		}
	}

	metadata_size = get_metadata_size_in_bytes(buffer);

	size_t metadata_size_with_header = 0;
	if (metadata_size > 0) {
		metadata_size_with_header = metadata_size + BLOCK_METADATA_HEADER_SIZE;
	}

	return size + metadata_size_with_header + BLOCK_TRAILING_MAGIC_SIZE;
}

SerializeResult serialize(const VoxelBufferInternal &voxel_buffer) {
	//
	VOXEL_PROFILE_SCOPE();

	std::vector<uint8_t> &dst_data = tls_data;
	std::vector<uint8_t> &metadata_tmp = tls_metadata_tmp;

	// Cannot serialize an empty block
	ERR_FAIL_COND_V(Vector3iUtil::get_volume(voxel_buffer.get_size()) == 0, SerializeResult(dst_data, false));

	size_t metadata_size = 0;
	const size_t data_size = get_size_in_bytes(voxel_buffer, metadata_size);
	dst_data.resize(data_size);

	FileAccessMemory file_access_memory;
	FileAccessMemory *f = &file_access_memory;
	ERR_FAIL_COND_V(f->open_custom(dst_data.data(), dst_data.size()) != OK, SerializeResult(dst_data, false));

	f->store_8(BLOCK_FORMAT_VERSION);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().x > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f->store_16(voxel_buffer.get_size().x);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().y > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f->store_16(voxel_buffer.get_size().y);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().z > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f->store_16(voxel_buffer.get_size().z);

	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		const VoxelBufferInternal::Compression compression = voxel_buffer.get_channel_compression(channel_index);
		const VoxelBufferInternal::Depth depth = voxel_buffer.get_channel_depth(channel_index);
		// Low nibble: compression (up to 16 values allowed)
		// High nibble: depth (up to 16 values allowed)
		const uint8_t fmt = static_cast<uint8_t>(compression) | (static_cast<uint8_t>(depth) << 4);
		f->store_8(fmt);

		switch (compression) {
			case VoxelBufferInternal::COMPRESSION_NONE: {
				Span<uint8_t> data;
				ERR_FAIL_COND_V(!voxel_buffer.get_channel_raw(channel_index, data), SerializeResult(dst_data, false));
				f->store_buffer(data.data(), data.size());
			} break;

			case VoxelBufferInternal::COMPRESSION_UNIFORM: {
				const uint64_t v = voxel_buffer.get_voxel(Vector3i(), channel_index);
				switch (depth) {
					case VoxelBufferInternal::DEPTH_8_BIT:
						f->store_8(v);
						break;
					case VoxelBufferInternal::DEPTH_16_BIT:
						f->store_16(v);
						break;
					case VoxelBufferInternal::DEPTH_32_BIT:
						f->store_32(v);
						break;
					case VoxelBufferInternal::DEPTH_64_BIT:
						f->store_64(v);
						break;
					default:
						CRASH_NOW();
				}
			} break;

			default:
				CRASH_COND("Unhandled compression mode");
		}
	}

	// Metadata has more reasons to fail. If a recoverable error occurs prior to serializing,
	// we just discard all metadata as if it was empty.
	if (metadata_size > 0) {
		f->store_32(metadata_size);
		metadata_tmp.resize(metadata_size);
		// This function brings me joy. </irony>
		serialize_metadata(metadata_tmp.data(), voxel_buffer, metadata_size);
		f->store_buffer(metadata_tmp.data(), metadata_tmp.size());
	}

	f->store_32(BLOCK_TRAILING_MAGIC);

	// Check out of bounds writing
	CRASH_COND(f->get_position() > dst_data.size());

	return SerializeResult(dst_data, true);
}

bool deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer) {
	VOXEL_PROFILE_SCOPE();

	std::vector<uint8_t> &metadata_tmp = tls_metadata_tmp;

	ERR_FAIL_COND_V(p_data.size() < sizeof(uint32_t), false);
	const uint32_t magic = *reinterpret_cast<const uint32_t *>(&p_data[p_data.size() - sizeof(uint32_t)]);
	ERR_FAIL_COND_V(magic != BLOCK_TRAILING_MAGIC, false);

	FileAccessMemory file_access_memory;
	FileAccessMemory *f = &file_access_memory;
	ERR_FAIL_COND_V(f->open_custom(p_data.data(), p_data.size()) != OK, false);

	const uint8_t format_version = f->get_8();
	if (format_version < 2) {
		// In version 1, the first thing coming in block data is the compression value of the first channel.
		// At the time, there was only 2 values this could take: 0 and 1.
		// So we can recognize blocks using this old format and seek back.
		// Formats before 2 also did not contain bit depth, they only had compression, leaving high nibble to 0.
		// This means version 2 will read only 8-bit depth from the old block.
		// "Fortunately", the old format also did not properly serialize formats using more than 8 bits.
		// So we are kinda set to migrate without much changes, by assuming the block is already formatted properly.
		f->seek(f->get_position() - 1);

		WARN_PRINT("Reading block format_version < 2. Attempting to migrate.");

	} else {
		ERR_FAIL_COND_V(format_version != BLOCK_FORMAT_VERSION, false);

		const unsigned int size_x = f->get_16();
		const unsigned int size_y = f->get_16();
		const unsigned int size_z = f->get_16();

		out_voxel_buffer.create(Vector3i(size_x, size_y, size_z));
	}

	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		const uint8_t fmt = f->get_8();
		const uint8_t compression_value = fmt & 0xf;
		const uint8_t depth_value = (fmt >> 4) & 0xf;
		ERR_FAIL_COND_V_MSG(compression_value >= VoxelBufferInternal::COMPRESSION_COUNT, false,
				"At offset 0x" + String::num_int64(f->get_position() - 1, 16));
		ERR_FAIL_COND_V_MSG(depth_value >= VoxelBufferInternal::DEPTH_COUNT, false,
				"At offset 0x" + String::num_int64(f->get_position() - 1, 16));
		VoxelBufferInternal::Compression compression = (VoxelBufferInternal::Compression)compression_value;
		VoxelBufferInternal::Depth depth = (VoxelBufferInternal::Depth)depth_value;

		out_voxel_buffer.set_channel_depth(channel_index, depth);

		switch (compression) {
			case VoxelBufferInternal::COMPRESSION_NONE: {
				out_voxel_buffer.decompress_channel(channel_index);

				Span<uint8_t> buffer;
				CRASH_COND(!out_voxel_buffer.get_channel_raw(channel_index, buffer));

				const uint32_t read_len = f->get_buffer(buffer.data(), buffer.size());
				if (read_len != buffer.size()) {
					ERR_PRINT("Unexpected end of file");
					return false;
				}

			} break;

			case VoxelBufferInternal::COMPRESSION_UNIFORM: {
				uint64_t v;
				switch (out_voxel_buffer.get_channel_depth(channel_index)) {
					case VoxelBufferInternal::DEPTH_8_BIT:
						v = f->get_8();
						break;
					case VoxelBufferInternal::DEPTH_16_BIT:
						v = f->get_16();
						break;
					case VoxelBufferInternal::DEPTH_32_BIT:
						v = f->get_32();
						break;
					case VoxelBufferInternal::DEPTH_64_BIT:
						v = f->get_64();
						break;
					default:
						CRASH_NOW();
				}
				out_voxel_buffer.clear_channel(channel_index, v);
			} break;

			default:
				ERR_PRINT("Unhandled compression mode");
				return false;
		}
	}

	if (p_data.size() - f->get_position() > BLOCK_TRAILING_MAGIC_SIZE) {
		const size_t metadata_size = f->get_32();
		ERR_FAIL_COND_V(f->get_position() + metadata_size > p_data.size(), false);
		metadata_tmp.resize(metadata_size);
		f->get_buffer(metadata_tmp.data(), metadata_tmp.size());
		deserialize_metadata(metadata_tmp.data(), out_voxel_buffer, metadata_tmp.size());
	}

	// Failure at this indicates file corruption
	ERR_FAIL_COND_V_MSG(
			f->get_32() != BLOCK_TRAILING_MAGIC, false, "At offset 0x" + String::num_int64(f->get_position() - 4, 16));
	return true;
}

SerializeResult serialize_and_compress(const VoxelBufferInternal &voxel_buffer) {
	VOXEL_PROFILE_SCOPE();

	std::vector<uint8_t> &compressed_data = tls_compressed_data;

	SerializeResult res = serialize(voxel_buffer);
	ERR_FAIL_COND_V(!res.success, SerializeResult(compressed_data, false));
	const std::vector<uint8_t> &data = res.data;

	res.success = CompressedData::compress(
			Span<const uint8_t>(data.data(), 0, data.size()), compressed_data, CompressedData::COMPRESSION_LZ4);
	ERR_FAIL_COND_V(!res.success, SerializeResult(compressed_data, false));

	return SerializeResult(compressed_data, true);
}

bool decompress_and_deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer) {
	VOXEL_PROFILE_SCOPE();

	std::vector<uint8_t> &data = tls_data;

	const bool res = CompressedData::decompress(p_data, data);
	ERR_FAIL_COND_V(!res, false);

	return deserialize(to_span_const(data), out_voxel_buffer);
}

bool decompress_and_deserialize(FileAccess *f, unsigned int size_to_read, VoxelBufferInternal &out_voxel_buffer) {
	VOXEL_PROFILE_SCOPE();
	ERR_FAIL_COND_V(f == nullptr, false);

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	const size_t fpos = f->get_position();
	const size_t remaining_file_size = f->get_length() - fpos;
	ERR_FAIL_COND_V(size_to_read > remaining_file_size, false);
#endif

	std::vector<uint8_t> &compressed_data = tls_compressed_data;

	compressed_data.resize(size_to_read);
	const unsigned int read_size = f->get_buffer(compressed_data.data(), size_to_read);
	ERR_FAIL_COND_V(read_size != size_to_read, false);

	return decompress_and_deserialize(to_span_const(compressed_data), out_voxel_buffer);
}

int serialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, bool compress) {
	if (compress) {
		SerializeResult res = serialize_and_compress(voxel_buffer);
		ERR_FAIL_COND_V(!res.success, -1);
		peer.put_data(res.data.data(), res.data.size());
		return res.data.size();

	} else {
		SerializeResult res = serialize(voxel_buffer);
		ERR_FAIL_COND_V(!res.success, -1);
		peer.put_data(res.data.data(), res.data.size());
		return res.data.size();
	}
}

void deserialize(StreamPeer &peer, VoxelBufferInternal &voxel_buffer, int size, bool decompress) {
	if (decompress) {
		std::vector<uint8_t> &compressed_data = tls_compressed_data;
		compressed_data.resize(size);
		const Error err = peer.get_data(compressed_data.data(), compressed_data.size());
		ERR_FAIL_COND(err != OK);
		bool success = decompress_and_deserialize(to_span_const(compressed_data), voxel_buffer);
		ERR_FAIL_COND(!success);

	} else {
		std::vector<uint8_t> &data = tls_data;
		data.resize(size);
		const Error err = peer.get_data(data.data(), data.size());
		ERR_FAIL_COND(err != OK);
		deserialize(to_span_const(data), voxel_buffer);
	}
}

} // namespace BlockSerializer
} // namespace zylann::voxel
