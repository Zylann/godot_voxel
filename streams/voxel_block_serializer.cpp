#include "voxel_block_serializer.h"
#include "../math/vector3i.h"
#include "../thirdparty/lz4/lz4.h"
#include "../voxel_buffer.h"
#include "../voxel_memory_pool.h"
#include <core/os/file_access.h>

namespace {
const unsigned int BLOCK_TRAILING_MAGIC = 0x900df00d;
const int BLOCK_TRAILING_MAGIC_SIZE = 4;
} // namespace

unsigned int VoxelBlockSerializer::get_size_in_bytes(const VoxelBuffer &buffer) {

	uint32_t size = 0;
	Vector3i size_in_voxels = buffer.get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		VoxelBuffer::Compression compression = buffer.get_channel_compression(channel_index);
		size += 1;

		switch (compression) {

			case VoxelBuffer::COMPRESSION_NONE: {
				size += size_in_voxels.volume() * sizeof(uint8_t);
			} break;

			case VoxelBuffer::COMPRESSION_UNIFORM: {
				size += 1;
			} break;

			default:
				ERR_PRINT("Unhandled compression mode");
				CRASH_NOW();
		}
	}

	return size + BLOCK_TRAILING_MAGIC_SIZE;
}

const std::vector<uint8_t> &VoxelBlockSerializer::serialize(VoxelBuffer &voxel_buffer) {

	unsigned int data_size = get_size_in_bytes(voxel_buffer);
	_data.resize(data_size);

	CRASH_COND(_file_access_memory.open_custom(_data.data(), _data.size()) != OK);
	FileAccessMemory *f = &_file_access_memory;

	Vector3i size_in_voxels = voxel_buffer.get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		VoxelBuffer::Compression compression = voxel_buffer.get_channel_compression(channel_index);
		f->store_8(static_cast<uint8_t>(compression));

		switch (compression) {

			case VoxelBuffer::COMPRESSION_NONE: {
				uint8_t *data = voxel_buffer.get_channel_raw(channel_index);
				CRASH_COND(data == nullptr);
				uint32_t len = size_in_voxels.volume() * sizeof(uint8_t);
				f->store_buffer(data, len);
			} break;

			case VoxelBuffer::COMPRESSION_UNIFORM: {
				int v = voxel_buffer.get_voxel(Vector3i(), channel_index);
				f->store_8((uint8_t)v);
			} break;

			default:
				CRASH_COND("Unhandled compression mode");
		}
	}

	f->store_32(BLOCK_TRAILING_MAGIC);

	return _data;
}

bool VoxelBlockSerializer::deserialize(const std::vector<uint8_t> &p_data, VoxelBuffer &out_voxel_buffer) {

	CRASH_COND(_file_access_memory.open_custom(p_data.data(), p_data.size()) != OK);
	FileAccessMemory *f = &_file_access_memory;

	Vector3i size_in_voxels = out_voxel_buffer.get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		uint8_t compression_value = f->get_8();
		ERR_EXPLAIN("At offset 0x" + String::num_int64(f->get_position() - 1, 16));
		ERR_FAIL_COND_V(compression_value >= VoxelBuffer::COMPRESSION_COUNT, false);
		VoxelBuffer::Compression compression = (VoxelBuffer::Compression)compression_value;

		switch (compression) {

			case VoxelBuffer::COMPRESSION_NONE: {
				uint32_t expected_len = size_in_voxels.volume() * sizeof(uint8_t);
				// TODO Optimize allocations here

				//uint8_t *buffer = (uint8_t *)memalloc(expected_len);
				// TODO TEMPORARY FIX, I got rid of grab_channel_data in the `depth` branch, this will go away once merged
				uint8_t *buffer = VoxelMemoryPool::get_singleton()->allocate(expected_len);

				uint32_t read_len = f->get_buffer(buffer, expected_len);
				if (read_len != expected_len) {
					memdelete(buffer);
					ERR_PRINT("Unexpected end of file");
					return false;
				}
				out_voxel_buffer.grab_channel_data(buffer, channel_index, compression);
			} break;

			case VoxelBuffer::COMPRESSION_UNIFORM:
				out_voxel_buffer.clear_channel(channel_index, f->get_8());
				break;

			default:
				ERR_PRINT("Unhandled compression mode");
				return false;
		}
	}

	// Failure at this indicates file corruption
	ERR_EXPLAIN("At offset 0x" + String::num_int64(f->get_position() - 4, 16));
	ERR_FAIL_COND_V(f->get_32() != BLOCK_TRAILING_MAGIC, false);
	return true;
}

const std::vector<uint8_t> &VoxelBlockSerializer::serialize_and_compress(VoxelBuffer &voxel_buffer) {

	const std::vector<uint8_t> &data = serialize(voxel_buffer);

	unsigned int header_size = sizeof(unsigned int);
	_compressed_data.resize(header_size + LZ4_compressBound(data.size()));

	// Write header
	CRASH_COND(_file_access_memory.open_custom(_compressed_data.data(), _compressed_data.size()) != OK);
	_file_access_memory.store_32(data.size());
	_file_access_memory.close();

	int compressed_size = LZ4_compress_default(
			(const char *)data.data(),
			(char *)_compressed_data.data() + header_size,
			data.size(),
			_compressed_data.size() - header_size);

	CRASH_COND(compressed_size < 0);
	CRASH_COND(compressed_size == 0);

	_compressed_data.resize(header_size + compressed_size);
	return _compressed_data;
}

bool VoxelBlockSerializer::decompress_and_deserialize(const std::vector<uint8_t> &p_data, VoxelBuffer &out_voxel_buffer) {

	// Read header
	unsigned int header_size = sizeof(unsigned int);
	ERR_FAIL_COND_V(_file_access_memory.open_custom(p_data.data(), p_data.size()) != OK, false);
	unsigned int decompressed_size = _file_access_memory.get_32();
	_file_access_memory.close();

	_data.resize(decompressed_size);

	unsigned int actually_decompressed_size = LZ4_decompress_safe(
			(const char *)p_data.data() + header_size,
			(char *)_data.data(),
			p_data.size() - header_size,
			_data.size());

	ERR_EXPLAIN(String("LZ4 decompression error {0}").format(varray(actually_decompressed_size)));
	ERR_FAIL_COND_V(actually_decompressed_size < 0, false);

	ERR_EXPLAIN(String("Expected {0} bytes, obtained {1}").format(varray(decompressed_size, actually_decompressed_size)));
	ERR_FAIL_COND_V(actually_decompressed_size != decompressed_size, false);

	return deserialize(_data, out_voxel_buffer);
}

bool VoxelBlockSerializer::decompress_and_deserialize(FileAccess *f, unsigned int size_to_read, VoxelBuffer &out_voxel_buffer) {

	ERR_FAIL_COND_V(f == nullptr, false);

	_compressed_data.resize(size_to_read);
	unsigned int read_size = f->get_buffer(_compressed_data.data(), size_to_read);
	ERR_FAIL_COND_V(read_size != size_to_read, false);

	return decompress_and_deserialize(_compressed_data, out_voxel_buffer);
}
