#include "voxel_stream_file.h"

Ref<VoxelStream> VoxelStreamFile::get_fallback_stream() const {
	return _fallback_stream;
}

void VoxelStreamFile::set_fallback_stream(Ref<VoxelStream> stream) {
	ERR_FAIL_COND(*stream == this);
	_fallback_stream = stream;
}

void VoxelStreamFile::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_fallback_stream", "stream"), &VoxelStreamFile::set_fallback_stream);
	ClassDB::bind_method(D_METHOD("get_fallback_stream"), &VoxelStreamFile::get_fallback_stream);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_fallback_stream", "get_fallback_stream");
}

void VoxelStreamFile::emerge_block_fallback(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	if (_fallback_stream.is_valid()) {
		_fallback_stream->emerge_block(out_buffer, origin_in_voxels, lod);
	}
}
