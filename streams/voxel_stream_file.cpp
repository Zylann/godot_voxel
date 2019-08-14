#include "voxel_stream_file.h"
#include <core/os/file_access.h>

namespace {
const int BLOCK_TRAILING_MAGIC = 0x900df00d;
const int BLOCK_TRAILING_MAGIC_SIZE = 4;
} // namespace

void VoxelStreamFile::set_save_fallback_output(bool enabled) {
	_save_fallback_output = enabled;
}

bool VoxelStreamFile::get_save_fallback_output() const {
	return _save_fallback_output;
}

Ref<VoxelStream> VoxelStreamFile::get_fallback_stream() const {
	return _fallback_stream;
}

void VoxelStreamFile::set_fallback_stream(Ref<VoxelStream> stream) {
	ERR_FAIL_COND(*stream == this);
	_fallback_stream = stream;
}

void VoxelStreamFile::emerge_block_fallback(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {

	// This function is just a helper around the true thing, really. I might remove it in the future.

	BlockRequest r;
	r.voxel_buffer = out_buffer;
	r.origin_in_voxels = origin_in_voxels;
	r.lod = lod;

	Vector<BlockRequest> requests;
	requests.push_back(r);

	emerge_blocks_fallback(requests);
}

void VoxelStreamFile::emerge_blocks_fallback(Vector<VoxelStreamFile::BlockRequest> &requests) {

	if (_fallback_stream.is_valid()) {

		_fallback_stream->emerge_blocks(requests);

		if (_save_fallback_output) {
			immerge_blocks(requests);
		}
	}
}

uint32_t VoxelStreamFile::get_voxel_buffer_size_in_bytes(Ref<VoxelBuffer> buffer) const {

	uint32_t size = 0;
	Vector3i size_in_voxels = buffer->get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		VoxelBuffer::Compression compression = buffer->get_channel_compression(channel_index);
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

// TODO Append 0x900df00d at the end of a serialized block to confirm data was read to expected length

bool VoxelStreamFile::read_voxel_buffer(FileAccess *f, Ref<VoxelBuffer> out_voxel_buffer) {

	Vector3i size_in_voxels = out_voxel_buffer->get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		uint8_t compression_value = f->get_8();
		ERR_FAIL_COND_V_MSG(compression_value >= VoxelBuffer::COMPRESSION_COUNT, false, "At offset 0x" + String::num_int64(f->get_position() - 1, 16));
		VoxelBuffer::Compression compression = (VoxelBuffer::Compression)compression_value;

		switch (compression) {

			case VoxelBuffer::COMPRESSION_NONE: {
				uint32_t expected_len = size_in_voxels.volume() * sizeof(uint8_t);
				// TODO Optimize allocations here
				uint8_t *buffer = (uint8_t *)memalloc(expected_len);
				uint32_t read_len = f->get_buffer(buffer, expected_len);
				if (read_len != expected_len) {
					memdelete(buffer);
					ERR_PRINT("Unexpected end of file");
					return false;
				}
				out_voxel_buffer->grab_channel_data(buffer, channel_index, compression);
			} break;

			case VoxelBuffer::COMPRESSION_UNIFORM:
				out_voxel_buffer->clear_channel(channel_index, f->get_8());
				break;

			default:
				ERR_PRINT("Unhandled compression mode");
				return false;
		}
	}

	// Failure at this indicates file corruption
	ERR_FAIL_COND_V_MSG(f->get_32() != BLOCK_TRAILING_MAGIC, false, "At offset 0x" + String::num_int64(f->get_position() - 4, 16));
	return true;
}

void VoxelStreamFile::write_voxel_buffer(FileAccess *f, Ref<VoxelBuffer> voxel_buffer) {

	Vector3i size_in_voxels = voxel_buffer->get_size();

	for (unsigned int channel_index = 0; channel_index < VoxelBuffer::MAX_CHANNELS; ++channel_index) {

		VoxelBuffer::Compression compression = voxel_buffer->get_channel_compression(channel_index);
		f->store_8(static_cast<uint8_t>(compression));

		switch (compression) {

			case VoxelBuffer::COMPRESSION_NONE: {
				uint8_t *data = voxel_buffer->get_channel_raw(channel_index);
				CRASH_COND(data == nullptr);
				uint32_t len = size_in_voxels.volume() * sizeof(uint8_t);
				f->store_buffer(data, len);
			} break;

			case VoxelBuffer::COMPRESSION_UNIFORM: {
				int v = voxel_buffer->get_voxel(Vector3i(), channel_index);
				f->store_8((uint8_t)v);
			} break;

			default:
				ERR_PRINT("Unhandled compression mode");
				return;
		}
	}

	f->store_32(BLOCK_TRAILING_MAGIC);
}

void VoxelStreamFile::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_save_fallback_output", "enabled"), &VoxelStreamFile::set_save_fallback_output);
	ClassDB::bind_method(D_METHOD("get_save_fallback_output"), &VoxelStreamFile::get_save_fallback_output);

	ClassDB::bind_method(D_METHOD("set_fallback_stream", "stream"), &VoxelStreamFile::set_fallback_stream);
	ClassDB::bind_method(D_METHOD("get_fallback_stream"), &VoxelStreamFile::get_fallback_stream);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_fallback_stream", "get_fallback_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_fallback_output"), "set_save_fallback_output", "get_save_fallback_output");
}
