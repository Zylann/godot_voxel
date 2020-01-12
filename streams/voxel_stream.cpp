#include "voxel_stream.h"
#include <core/script_language.h>

VoxelStream::VoxelStream() {
}

void VoxelStream::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(out_buffer.is_null());
	ScriptInstance *script = get_script_instance();
	if (script) {
		// Call script to generate buffer
		Variant arg1 = out_buffer;
		Variant arg2 = origin_in_voxels.to_vec3();
		Variant arg3 = lod;
		const Variant *args[3] = { &arg1, &arg2, &arg3 };
		Variant::CallError err;
		script->call("emerge_block", args, 3, err);
		ERR_EXPLAIN("voxel_stream.cpp:emerge_block gave an error: " + String::num(err.error) +
						", Argument: " + String::num(err.argument) +
						", Expected type: " + Variant::get_type_name(err.expected));
		ERR_FAIL_COND(err.error != Variant::CallError::CALL_OK);
		// This had to be explicitely logged due to the usual GD debugger not working with threads
	}
}

void VoxelStream::immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(buffer.is_null());
	ScriptInstance *script = get_script_instance();
	if (script) {
		// Call script to save buffer
		Variant arg1 = buffer;
		Variant arg2 = origin_in_voxels.to_vec3();
		Variant arg3 = lod;
		const Variant *args[3] = { &arg1, &arg2, &arg3 };
		Variant::CallError err;
		script->call("immerge_block", args, 3, err);
		ERR_EXPLAIN("voxel_stream.cpp:immerge_block gave an error: " + String::num(err.error) +
						" Argument: " + String::num(err.argument) +
						" Expected type: " + Variant::get_type_name(err.expected));
		ERR_FAIL_COND(err.error != Variant::CallError::CALL_OK);
		// This had to be explicitely logged due to the usual GD debugger not working with threads
	}
}

void VoxelStream::emerge_blocks(Vector<VoxelStream::BlockRequest> &p_blocks) {
	// Default implementation. May matter for some stream types to optimize loading.
	for (int i = 0; i < p_blocks.size(); ++i) {
		BlockRequest &r = p_blocks.write[i];
		emerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
	}
}

void VoxelStream::immerge_blocks(Vector<VoxelStream::BlockRequest> &p_blocks) {
	for (int i = 0; i < p_blocks.size(); ++i) {
		BlockRequest &r = p_blocks.write[i];
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

VoxelStream::Stats VoxelStream::get_statistics() const {
	return _stats;
}

void VoxelStream::_bind_methods() {
	// Note: C++ inheriting classes don't need to re-bind these, because they are bindings that call the actual virtual methods

	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelStream::_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer", "origin_in_voxels", "lod"), &VoxelStream::_immerge_block);
}
