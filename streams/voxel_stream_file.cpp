#include "voxel_stream_file.h"
#include <core/os/file_access.h>

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

void VoxelStreamFile::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_save_fallback_output", "enabled"), &VoxelStreamFile::set_save_fallback_output);
	ClassDB::bind_method(D_METHOD("get_save_fallback_output"), &VoxelStreamFile::get_save_fallback_output);

	ClassDB::bind_method(D_METHOD("set_fallback_stream", "stream"), &VoxelStreamFile::set_fallback_stream);
	ClassDB::bind_method(D_METHOD("get_fallback_stream"), &VoxelStreamFile::get_fallback_stream);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_fallback_stream", "get_fallback_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_fallback_output"), "set_save_fallback_output", "get_save_fallback_output");
}
