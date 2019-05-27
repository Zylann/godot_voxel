#include "voxel_stream.h"
#include <core/script_language.h>

void VoxelStream::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(out_buffer.is_null());
	ScriptInstance *script = get_script_instance();
	if (script) {
		// Call script to generate buffer
		Variant arg1 = out_buffer;
		Variant arg2 = origin_in_voxels.to_vec3();
		Variant arg3 = lod;
		const Variant *args[3] = { &arg1, &arg2, &arg3 };
		//Variant::CallError err; // wut
		script->call("emerge_block", args, 3);
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
		//Variant::CallError err; // wut
		script->call("immerge_block", args, 3);
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

void VoxelStream::_bind_methods() {
	// Note: C++ inheriting classes don't need to re-bind these, because they are bindings that call the actual virtual methods

	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelStream::_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer", "origin_in_voxels", "lod"), &VoxelStream::_immerge_block);
}
