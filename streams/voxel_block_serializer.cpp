#include "voxel_block_serializer.h"
#include "../storage/voxel_buffer_internal.h"
#include "../storage/voxel_memory_pool.h"
#include "../util/container_funcs.h"
#include "../util/dstack.h"
#include "../util/godot/classes/file.h"
#include "../util/macros.h"
#include "../util/math/vector3i.h"
#include "../util/profiling.h"
#include "../util/serialization.h"
#include "../util/string_funcs.h"
#include "compressed_data.h"

#if defined(ZN_GODOT) || defined(ZN_GODOT_EXTENSION)
#include "../storage/voxel_metadata_variant.h"
#endif

#include <limits>

namespace zylann::voxel {
namespace BlockSerializer {

const unsigned int BLOCK_TRAILING_MAGIC = 0x900df00d;
const unsigned int BLOCK_TRAILING_MAGIC_SIZE = 4;
const unsigned int BLOCK_METADATA_HEADER_SIZE = sizeof(uint32_t);

// Temporary data buffers, re-used to reduce allocations

std::vector<uint8_t> &get_tls_metadata_tmp() {
	thread_local std::vector<uint8_t> tls_metadata_tmp;
	return tls_metadata_tmp;
}

std::vector<uint8_t> &get_tls_data() {
	thread_local std::vector<uint8_t> tls_data;
	return tls_data;
}

std::vector<uint8_t> &get_tls_compressed_data() {
	thread_local std::vector<uint8_t> tls_compressed_data;
	return tls_compressed_data;
}

size_t get_metadata_size_in_bytes(const VoxelMetadata &meta) {
	size_t size = 1; // Type
	switch (meta.get_type()) {
		case VoxelMetadata::TYPE_EMPTY:
			break;
		case VoxelMetadata::TYPE_U64:
			size += sizeof(uint64_t);
			break;
		default:
			if (meta.get_type() >= VoxelMetadata::TYPE_CUSTOM_BEGIN) {
				const ICustomVoxelMetadata &custom = meta.get_custom();
				size += custom.get_serialized_size();
			} else {
				ZN_PRINT_ERROR("Unknown metadata type");
				return 0;
			}
	}
	return size;
}

size_t get_metadata_size_in_bytes(const VoxelBufferInternal &buffer) {
	size_t size = 0;

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &voxel_metadata = buffer.get_voxel_metadata();
	for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator it = voxel_metadata.begin();
			it != voxel_metadata.end(); ++it) {
		const Vector3i pos = it->key;

		ERR_FAIL_COND_V_MSG(pos.x < 0 || static_cast<uint32_t>(pos.x) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata X position");
		ERR_FAIL_COND_V_MSG(pos.y < 0 || static_cast<uint32_t>(pos.y) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata Y position");
		ERR_FAIL_COND_V_MSG(pos.z < 0 || static_cast<uint32_t>(pos.z) >= VoxelBufferInternal::MAX_SIZE, 0,
				"Invalid voxel metadata Z position");

		size += 3 * sizeof(uint16_t); // Positions are stored as 3 unsigned shorts
		size += get_metadata_size_in_bytes(it->value);
	}

	// If no metadata is found at all, nothing is serialized, not even null.
	// It spares 24 bytes (40 if real_t == double),
	// and is backward compatible with saves made before introduction of metadata.

	const VoxelMetadata &block_meta = buffer.get_block_metadata();

	if (size != 0 || block_meta.get_type() != VoxelMetadata::TYPE_EMPTY) {
		size += get_metadata_size_in_bytes(block_meta);
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

static void serialize_metadata(const VoxelMetadata &meta, MemoryWriterExistingBuffer &mw) {
	const uint8_t type = meta.get_type();
	switch (type) {
		case VoxelMetadata::TYPE_EMPTY:
			mw.store_8(type);
			break;
		case VoxelMetadata::TYPE_U64:
			mw.store_8(type);
			mw.store_64(meta.get_u64());
			break;
		default:
			if (type >= VoxelMetadata::TYPE_CUSTOM_BEGIN) {
				mw.store_8(type);
				const size_t written_size = meta.get_custom().serialize(mw.data.data.sub(mw.data.pos));
				ZN_ASSERT(mw.data.pos + written_size <= mw.data.data.size());
				mw.data.pos += written_size;
			} else {
				ZN_PRINT_ERROR("Unknown metadata type");
				mw.store_8(VoxelMetadata::TYPE_EMPTY);
			}
			break;
	}
}

// The target buffer MUST have correct size. Recoverable errors must have been checked before.
void serialize_metadata(Span<uint8_t> p_dst, const VoxelBufferInternal &buffer) {
	ByteSpanWithPosition bs(p_dst, 0);
	MemoryWriterExistingBuffer mw(bs, ENDIANESS_LITTLE_ENDIAN);

	const VoxelMetadata &block_meta = buffer.get_block_metadata();
	serialize_metadata(block_meta, mw);

	const FlatMapMoveOnly<Vector3i, VoxelMetadata> &voxel_metadata = buffer.get_voxel_metadata();
	for (FlatMapMoveOnly<Vector3i, VoxelMetadata>::ConstIterator it = voxel_metadata.begin();
			it != voxel_metadata.end(); ++it) {
		// Serializing key as ushort because it's more than enough for a 3D dense array
		static_assert(VoxelBufferInternal::MAX_SIZE <= std::numeric_limits<uint16_t>::max(),
				"Maximum size exceeds serialization support");
		const Vector3i pos = it->key;
		mw.store_16(pos.x);
		mw.store_16(pos.y);
		mw.store_16(pos.z);

		serialize_metadata(it->value, mw);
	}
}

template <typename T>
struct ClearOnExit {
	T &container;
	~ClearOnExit() {
		container.clear();
	}
};

//#define CLEAR_ON_EXIT(container) ClearOnExit<decltype(container)> clear_on_exit_##__LINE__;

static bool deserialize_metadata(VoxelMetadata &meta, MemoryReader &mr) {
	const uint8_t type = mr.get_8();
	switch (type) {
		case VoxelMetadata::TYPE_EMPTY:
			meta.clear();
			return true;

		case VoxelMetadata::TYPE_U64:
			meta.set_u64(mr.get_64());
			return true;

		default:
			if (type >= VoxelMetadata::TYPE_CUSTOM_BEGIN) {
				ICustomVoxelMetadata *custom = VoxelMetadataFactory::get_singleton().try_construct(type);
				ZN_ASSERT_RETURN_V_MSG(
						custom != nullptr, false, format("Could not deserialize custom metadata with type {}", type));

				// Store in a temporary container so it auto-deletes in case of error
				VoxelMetadata temp;
				temp.set_custom(type, custom);

				uint64_t read_size = 0;
				ZN_ASSERT_RETURN_V(custom->deserialize(mr.data.sub(mr.pos), read_size), false);
				ZN_ASSERT_RETURN_V(mr.pos + read_size <= mr.data.size(), false);
				mr.pos += read_size;

				meta = std::move(temp);
				return true;

			} else {
				ZN_PRINT_ERROR("Unknown metadata type");
				return false;
			}
	}
	return false;
}

bool deserialize_metadata(Span<const uint8_t> p_src, VoxelBufferInternal &buffer) {
	MemoryReader mr(p_src, ENDIANESS_LITTLE_ENDIAN);

	ZN_ASSERT_RETURN_V(deserialize_metadata(buffer.get_block_metadata(), mr), false);

	typedef FlatMapMoveOnly<Vector3i, VoxelMetadata>::Pair Pair;
	static thread_local std::vector<Pair> tls_pairs;
	// Clear when exiting scope (including cases of error) so we don't store dangling Variants
	ClearOnExit<std::vector<Pair>> clear_tls_pairs{ tls_pairs };

	while (mr.pos < mr.data.size()) {
		Vector3i pos;
		pos.x = mr.get_16();
		pos.y = mr.get_16();
		pos.z = mr.get_16();

		ZN_ASSERT_CONTINUE_MSG(buffer.is_position_valid(pos),
				format("Invalid voxel metadata position {} for buffer of size {}", pos, buffer.get_size()));

		// VoxelMetadata &vmeta = buffer.get_or_create_voxel_metadata(pos);
		tls_pairs.resize(tls_pairs.size() + 1);
		Pair &p = tls_pairs.back();
		p.key = pos;
		ZN_ASSERT_RETURN_V_MSG(
				deserialize_metadata(p.value, mr), false, format("Failed to deserialize voxel metadata {}", pos));
	}

	// Set all metadata at once, FlatMap is faster to initialize this way
	buffer.clear_and_set_voxel_metadata(to_span(tls_pairs));

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
	ZN_PROFILE_SCOPE();

	std::vector<uint8_t> &dst_data = get_tls_data();
	std::vector<uint8_t> &metadata_tmp = get_tls_metadata_tmp();
	dst_data.clear();
	metadata_tmp.clear();

	// Cannot serialize an empty block
	ERR_FAIL_COND_V(Vector3iUtil::get_volume(voxel_buffer.get_size()) == 0, SerializeResult(dst_data, false));

	size_t expected_metadata_size = 0;
	const size_t expected_data_size = get_size_in_bytes(voxel_buffer, expected_metadata_size);
	dst_data.reserve(expected_data_size);

	MemoryWriter f(dst_data, ENDIANESS_LITTLE_ENDIAN);

	f.store_8(BLOCK_FORMAT_VERSION);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().x > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f.store_16(voxel_buffer.get_size().x);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().y > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f.store_16(voxel_buffer.get_size().y);

	ERR_FAIL_COND_V(
			voxel_buffer.get_size().z > std::numeric_limits<uint16_t>().max(), SerializeResult(dst_data, false));
	f.store_16(voxel_buffer.get_size().z);

	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		const VoxelBufferInternal::Compression compression = voxel_buffer.get_channel_compression(channel_index);
		const VoxelBufferInternal::Depth depth = voxel_buffer.get_channel_depth(channel_index);
		// Low nibble: compression (up to 16 values allowed)
		// High nibble: depth (up to 16 values allowed)
		const uint8_t fmt = static_cast<uint8_t>(compression) | (static_cast<uint8_t>(depth) << 4);
		f.store_8(fmt);

		switch (compression) {
			case VoxelBufferInternal::COMPRESSION_NONE: {
				Span<uint8_t> data;
				ERR_FAIL_COND_V(!voxel_buffer.get_channel_raw(channel_index, data), SerializeResult(dst_data, false));
				f.store_buffer(data);
			} break;

			case VoxelBufferInternal::COMPRESSION_UNIFORM: {
				const uint64_t v = voxel_buffer.get_voxel(Vector3i(), channel_index);
				switch (depth) {
					case VoxelBufferInternal::DEPTH_8_BIT:
						f.store_8(v);
						break;
					case VoxelBufferInternal::DEPTH_16_BIT:
						f.store_16(v);
						break;
					case VoxelBufferInternal::DEPTH_32_BIT:
						f.store_32(v);
						break;
					case VoxelBufferInternal::DEPTH_64_BIT:
						f.store_64(v);
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
	if (expected_metadata_size > 0) {
		f.store_32(expected_metadata_size);
		metadata_tmp.resize(expected_metadata_size);
		// This function brings me joy. </irony>
		serialize_metadata(to_span(metadata_tmp), voxel_buffer);
		f.store_buffer(to_span(metadata_tmp));
	}

	f.store_32(BLOCK_TRAILING_MAGIC);

	// Check out of bounds writing
	CRASH_COND(dst_data.size() != expected_data_size);

	return SerializeResult(dst_data, true);
}

namespace legacy {

bool migrate_v3_to_v4(Span<const uint8_t> p_data, std::vector<uint8_t> &dst) {
	// In v3, metadata was always a Godot Variant. In v4, metadata uses an independent format.

#if defined(ZN_GODOT) || defined(ZN_GODOT_EXTENSION)

	// Constants used at the time of this version
	const unsigned int channel_count = 8;
	const unsigned int no_compression = 0;
	const unsigned int uniform_compression = 1;

	MemoryReader mr(p_data, ENDIANESS_LITTLE_ENDIAN);

	const uint8_t rv = mr.get_8(); // version
	ZN_ASSERT(rv == 3);

	const uint16_t size_x = mr.get_16(); // size_x
	const uint16_t size_y = mr.get_16(); // size_y
	const uint16_t size_z = mr.get_16(); // size_z
	const unsigned int volume = size_x * size_y * size_z;

	for (unsigned int channel_index = 0; channel_index < channel_count; ++channel_index) {
		const uint8_t fmt = mr.get_8();

		const uint8_t compression_value = fmt & 0xf;
		const uint8_t depth_value = (fmt >> 4) & 0xf;

		ZN_ASSERT_RETURN_V(compression_value < 2, false);
		ZN_ASSERT_RETURN_V(depth_value < 4, false);

		if (compression_value == no_compression) {
			mr.pos += volume << depth_value;

		} else if (compression_value == uniform_compression) {
			mr.pos += size_t(1) << depth_value;
		}
	}

	ZN_ASSERT(mr.pos <= mr.data.size());

	// Copy everything up to beginning of metadata
	dst.resize(mr.pos);
	memcpy(dst.data(), p_data.data(), mr.pos);
	// Set version
	dst[0] = 4;

	// Convert metadata

	const size_t total_metadata_size = mr.data.size() - mr.pos;

	if (total_metadata_size > 0) {
		MemoryWriter mw(dst, ENDIANESS_LITTLE_ENDIAN);

		struct L {
			static bool convert_metadata_item(MemoryReader &mr, MemoryWriter &mw) {
				// Read Variant
				Variant src_meta;
				size_t read_length;
				const bool decode_success = decode_variant(
						Span<const uint8_t>(&mr.data[mr.pos], mr.data.size() - mr.pos), src_meta, read_length);
				ZN_ASSERT_RETURN_V_MSG(decode_success, false, "Failed to deserialize v3 Variant metadata");
				mr.pos += read_length;
				ZN_ASSERT(mr.pos <= mr.data.size());

				// Write v4 equivalent
				VoxelMetadata dst_meta;
				gd::VoxelMetadataVariant *custom = ZN_NEW(gd::VoxelMetadataVariant);
				custom->data = src_meta;
				dst_meta.set_custom(gd::METADATA_TYPE_VARIANT, custom);
				mw.store_8(dst_meta.get_type());
				const size_t ss = custom->get_serialized_size();
				const size_t prev_size = mw.data.size();
				mw.data.resize(mw.data.size() + ss);
				const size_t written_size = custom->serialize(Span<uint8_t>(mw.data.data() + prev_size, ss));
				mw.data.resize(prev_size + written_size);

				return true;
			}
		};

		ZN_ASSERT_RETURN_V(L::convert_metadata_item(mr, mw), false);

		while (mr.pos < mr.data.size()) {
			const uint16_t pos_x = mr.get_16();
			const uint16_t pos_y = mr.get_16();
			const uint16_t pos_z = mr.get_16();

			mw.store_16(pos_x);
			mw.store_16(pos_y);
			mw.store_16(pos_z);

			ZN_ASSERT_RETURN_V(L::convert_metadata_item(mr, mw), false);
		}
	}

#else
	ZN_PRINT_ERROR("Cannot migrate block from v3 to v4, Godot Engine is required");
	return false;

#endif
	return true;
}

bool migrate_v2_to_v3(Span<const uint8_t> p_data, std::vector<uint8_t> &dst) {
	// In v2, SDF data was using a legacy arbitrary formula to encode fixed-point numbers.
	// In v3, it now uses inorm8 and inorm16.
	// Serialized size does not change.

	// Constants used at the time of this version
	const unsigned int channel_count = 8;
	const unsigned int sdf_channel_index = 2;
	const unsigned int no_compression = 0;
	const unsigned int uniform_compression = 1;

	dst.resize(p_data.size());
	memcpy(dst.data(), p_data.data(), p_data.size());

	MemoryReader mr(p_data, ENDIANESS_LITTLE_ENDIAN);

	const uint8_t rv = mr.get_8(); // version
	ZN_ASSERT(rv == 2);

	dst[0] = 3;

	const unsigned short size_x = mr.get_16(); // size_x
	const unsigned short size_y = mr.get_16(); // size_y
	const unsigned short size_z = mr.get_16(); // size_z
	const unsigned int volume = size_x * size_y * size_z;

	for (unsigned int channel_index = 0; channel_index < channel_count; ++channel_index) {
		const uint8_t fmt = mr.get_8();
		const uint8_t compression_value = fmt & 0xf;
		const uint8_t depth_value = (fmt >> 4) & 0xf;

		ERR_FAIL_INDEX_V(compression_value, 2, false);
		ERR_FAIL_INDEX_V(depth_value, 4, false);

		const unsigned int voxel_size = 1 << depth_value;

		if (channel_index == sdf_channel_index) {
			ByteSpanWithPosition dst2(to_span(dst), mr.pos);
			MemoryWriterExistingBuffer mw(dst2, ENDIANESS_LITTLE_ENDIAN);

			if (compression_value == no_compression) {
				switch (depth_value) {
					case 0:
						for (unsigned int i = 0; i < volume; ++i) {
							mw.store_8(snorm_to_s8(voxel::legacy::u8_to_snorm(mr.get_8())));
						}
						break;
					case 1:
						for (unsigned int i = 0; i < volume; ++i) {
							mw.store_16(snorm_to_s16(voxel::legacy::u16_to_snorm(mr.get_16())));
						}
						break;
					case 2:
					case 3:
						// Depths above 16bit use floats, just skip them
						mr.pos += voxel_size * volume;
						break;
				}
			} else if (compression_value == uniform_compression) {
				switch (depth_value) {
					case 0:
						mw.store_8(snorm_to_s8(voxel::legacy::u8_to_snorm(mr.get_8())));
						break;
					case 1:
						mw.store_16(snorm_to_s16(voxel::legacy::u16_to_snorm(mr.get_16())));
						break;
					case 2:
					case 3:
						// Depths above 16bit use floats, just skip them
						mr.pos += voxel_size;
						break;
				}
			}

		} else {
			// Skip
			if (compression_value == no_compression) {
				mr.pos += voxel_size * volume;
			} else if (compression_value == uniform_compression) {
				mr.pos += voxel_size;
			}
		}
	}

	return true;
}

} // namespace legacy

bool deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer) {
	ZN_DSTACK();
	ZN_PROFILE_SCOPE();

	std::vector<uint8_t> &metadata_tmp = get_tls_metadata_tmp();

	ERR_FAIL_COND_V(p_data.size() < sizeof(uint32_t), false);
	const uint32_t magic = *reinterpret_cast<const uint32_t *>(&p_data[p_data.size() - sizeof(uint32_t)]);
#if DEV_ENABLED
	if (magic != BLOCK_TRAILING_MAGIC) {
		print_data_hex(p_data);
	}
#endif
	ERR_FAIL_COND_V(magic != BLOCK_TRAILING_MAGIC, false);

	MemoryReader f(p_data, ENDIANESS_LITTLE_ENDIAN);

	const uint8_t format_version = f.get_8();

	switch (format_version) {
		case 2: {
			std::vector<uint8_t> migrated_data;
			ERR_FAIL_COND_V(!legacy::migrate_v2_to_v3(p_data, migrated_data), false);
			return deserialize(to_span(migrated_data), out_voxel_buffer);
		} break;

		case 3: {
			std::vector<uint8_t> migrated_data;
			ERR_FAIL_COND_V(!legacy::migrate_v3_to_v4(p_data, migrated_data), false);
			return deserialize(to_span(migrated_data), out_voxel_buffer);
		} break;

		default:
			ERR_FAIL_COND_V(format_version != BLOCK_FORMAT_VERSION, false);
	}

	const unsigned int size_x = f.get_16();
	const unsigned int size_y = f.get_16();
	const unsigned int size_z = f.get_16();

	out_voxel_buffer.create(Vector3i(size_x, size_y, size_z));

	for (unsigned int channel_index = 0; channel_index < VoxelBufferInternal::MAX_CHANNELS; ++channel_index) {
		const uint8_t fmt = f.get_8();
		const uint8_t compression_value = fmt & 0xf;
		const uint8_t depth_value = (fmt >> 4) & 0xf;
		ERR_FAIL_COND_V_MSG(compression_value >= VoxelBufferInternal::COMPRESSION_COUNT, false,
				"At offset 0x" + String::num_int64(f.get_position() - 1, 16));
		ERR_FAIL_COND_V_MSG(depth_value >= VoxelBufferInternal::DEPTH_COUNT, false,
				"At offset 0x" + String::num_int64(f.get_position() - 1, 16));
		VoxelBufferInternal::Compression compression = (VoxelBufferInternal::Compression)compression_value;
		VoxelBufferInternal::Depth depth = (VoxelBufferInternal::Depth)depth_value;

		out_voxel_buffer.set_channel_depth(channel_index, depth);

		switch (compression) {
			case VoxelBufferInternal::COMPRESSION_NONE: {
				out_voxel_buffer.decompress_channel(channel_index);

				Span<uint8_t> buffer;
				CRASH_COND(!out_voxel_buffer.get_channel_raw(channel_index, buffer));

				const size_t read_len = f.get_buffer(buffer);
				if (read_len != buffer.size()) {
					ERR_PRINT("Unexpected end of file");
					return false;
				}

			} break;

			case VoxelBufferInternal::COMPRESSION_UNIFORM: {
				uint64_t v;
				switch (out_voxel_buffer.get_channel_depth(channel_index)) {
					case VoxelBufferInternal::DEPTH_8_BIT:
						v = f.get_8();
						break;
					case VoxelBufferInternal::DEPTH_16_BIT:
						v = f.get_16();
						break;
					case VoxelBufferInternal::DEPTH_32_BIT:
						v = f.get_32();
						break;
					case VoxelBufferInternal::DEPTH_64_BIT:
						v = f.get_64();
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

	if (p_data.size() - f.get_position() > BLOCK_TRAILING_MAGIC_SIZE) {
		const size_t metadata_size = f.get_32();
		ERR_FAIL_COND_V(f.get_position() + metadata_size > p_data.size(), false);
		metadata_tmp.resize(metadata_size);
		f.get_buffer(to_span(metadata_tmp));
		deserialize_metadata(to_span(metadata_tmp), out_voxel_buffer);
	}

	// Failure at this indicates file corruption
	ERR_FAIL_COND_V_MSG(
			f.get_32() != BLOCK_TRAILING_MAGIC, false, "At offset 0x" + String::num_int64(f.get_position() - 4, 16));
	return true;
}

SerializeResult serialize_and_compress(const VoxelBufferInternal &voxel_buffer) {
	ZN_PROFILE_SCOPE();

	std::vector<uint8_t> &compressed_data = get_tls_compressed_data();

	SerializeResult res = serialize(voxel_buffer);
	ERR_FAIL_COND_V(!res.success, SerializeResult(compressed_data, false));
	const std::vector<uint8_t> &data = res.data;

	res.success = CompressedData::compress(
			Span<const uint8_t>(data.data(), 0, data.size()), compressed_data, CompressedData::COMPRESSION_LZ4);
	ERR_FAIL_COND_V(!res.success, SerializeResult(compressed_data, false));

	return SerializeResult(compressed_data, true);
}

bool decompress_and_deserialize(Span<const uint8_t> p_data, VoxelBufferInternal &out_voxel_buffer) {
	ZN_PROFILE_SCOPE();

	std::vector<uint8_t> &data = get_tls_data();

	const bool res = CompressedData::decompress(p_data, data);
	ERR_FAIL_COND_V(!res, false);

	return deserialize(to_span_const(data), out_voxel_buffer);
}

bool decompress_and_deserialize(FileAccess &f, unsigned int size_to_read, VoxelBufferInternal &out_voxel_buffer) {
	ZN_PROFILE_SCOPE();

#if defined(TOOLS_ENABLED) || defined(DEBUG_ENABLED)
	const size_t fpos = f.get_position();
	const size_t remaining_file_size = f.get_length() - fpos;
	ERR_FAIL_COND_V(size_to_read > remaining_file_size, false);
#endif

	std::vector<uint8_t> &compressed_data = get_tls_compressed_data();

	compressed_data.resize(size_to_read);
	const unsigned int read_size = get_buffer(f, to_span(compressed_data));
	ERR_FAIL_COND_V(read_size != size_to_read, false);

	return decompress_and_deserialize(to_span(compressed_data), out_voxel_buffer);
}

} // namespace BlockSerializer
} // namespace zylann::voxel
