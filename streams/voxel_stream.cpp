#include "voxel_stream.h"
#include "../voxel_string_names.h"
#include <core/script_language.h>

VoxelStream::VoxelStream() {
}

void VoxelStream::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(out_buffer.is_null());
	try_call_script(this, VoxelStringNames::get_singleton()->emerge_block, out_buffer, origin_in_voxels.to_vec3(), lod, nullptr);
}

void VoxelStream::immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(buffer.is_null());
	try_call_script(this, VoxelStringNames::get_singleton()->immerge_block, buffer, origin_in_voxels.to_vec3(), lod, nullptr);
}

void VoxelStream::emerge_blocks(Vector<VoxelBlockRequest> &p_blocks) {
	// Default implementation. May matter for some stream types to optimize loading.
	for (int i = 0; i < p_blocks.size(); ++i) {
		VoxelBlockRequest &r = p_blocks.write[i];
		emerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
	}
}

void VoxelStream::immerge_blocks(Vector<VoxelBlockRequest> &p_blocks) {
	for (int i = 0; i < p_blocks.size(); ++i) {
		VoxelBlockRequest &r = p_blocks.write[i];
		immerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
	}
}

bool VoxelStream::is_thread_safe() const {
	return false;
}

bool VoxelStream::is_cloneable() const {
	return false;
}

void VoxelStream::_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	emerge_block(out_buffer, Vector3i(origin_in_voxels), lod);
}

void VoxelStream::_immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	immerge_block(buffer, Vector3i(origin_in_voxels), lod);
}

int VoxelStream::get_used_channels_mask() const {
	Variant ret;
	if (try_call_script(this, VoxelStringNames::get_singleton()->get_used_channels_mask, nullptr, 0, &ret)) {
		return ret;
	}
	return 0;
}

int VoxelStream::_get_used_channels_mask() const {
	return get_used_channels_mask();
}

VoxelStream::Stats VoxelStream::get_statistics() const {
	return _stats;
}

bool VoxelStream::has_script() const {
	Ref<Script> s = get_script();
	return s.is_valid();
}

void VoxelStream::_bind_methods() {
	// TODO Make these proper virtual, it confuses C# bindings
	// Note: C++ inheriting classes don't need to re-bind these, because they are bindings that call the actual virtual methods
	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelStream::_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer", "origin_in_voxels", "lod"), &VoxelStream::_immerge_block);
	ClassDB::bind_method(D_METHOD("get_used_channels_mask"), &VoxelStream::_get_used_channels_mask);
}
