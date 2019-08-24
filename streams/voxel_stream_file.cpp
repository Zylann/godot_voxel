#include "voxel_stream_file.h"
#include <core/os/file_access.h>
#include <core/os/os.h>

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
	VOXEL_PROFILE_SCOPE(profile_scope);

	if (_fallback_stream.is_valid()) {

		_fallback_stream->emerge_blocks(requests);

		if (_save_fallback_output) {
			immerge_blocks(requests);
		}
	}
}

FileAccess *VoxelStreamFile::open_file(const String &fpath, int mode_flags, Error *err) {
	VOXEL_PROFILE_SCOPE(profile_scope);
	uint64_t time_before = OS::get_singleton()->get_ticks_usec();
	FileAccess *f = FileAccess::open(fpath, mode_flags, err);
	uint64_t time_spent = OS::get_singleton()->get_ticks_usec() - time_before;
	_stats.time_spent_opening_files += time_spent;
	++_stats.file_openings;
	return f;
}

int VoxelStreamFile::get_block_size_po2() const {
	return 4;
}

int VoxelStreamFile::get_lod_count() const {
	return 1;
}

Vector3 VoxelStreamFile::_get_block_size() const {
	return Vector3i(1 << get_block_size_po2()).to_vec3();
}

void VoxelStreamFile::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_save_fallback_output", "enabled"), &VoxelStreamFile::set_save_fallback_output);
	ClassDB::bind_method(D_METHOD("get_save_fallback_output"), &VoxelStreamFile::get_save_fallback_output);

	ClassDB::bind_method(D_METHOD("set_fallback_stream", "stream"), &VoxelStreamFile::set_fallback_stream);
	ClassDB::bind_method(D_METHOD("get_fallback_stream"), &VoxelStreamFile::get_fallback_stream);

	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelStreamFile::_get_block_size);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_stream", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStream"), "set_fallback_stream", "get_fallback_stream");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_fallback_output"), "set_save_fallback_output", "get_save_fallback_output");
}
