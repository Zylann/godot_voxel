#include "voxel_provider.h"
#include <core/script_language.h>

void VoxelProvider::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels) {
	ERR_FAIL_COND(out_buffer.is_null());
	ScriptInstance *script = get_script_instance();
	if (script) {
		// Call script to generate buffer
		Variant arg1 = out_buffer;
		Variant arg2 = origin_in_voxels.to_vec3();
		const Variant *args[2] = { &arg1, &arg2 };
		//Variant::CallError err; // wut
		script->call_multilevel("emerge_block", args, 2);
	}
}

void VoxelProvider::immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels) {
	ERR_FAIL_COND(buffer.is_null());
	ScriptInstance *script = get_script_instance();
	if (script) {
		// Call script to save buffer
		Variant arg1 = buffer;
		Variant arg2 = origin_in_voxels.to_vec3();
		const Variant *args[2] = { &arg1, &arg2 };
		//Variant::CallError err; // wut
		script->call_multilevel("immerge_block", args, 2);
	}
}

void VoxelProvider::_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels) {
	emerge_block(out_buffer, Vector3i(origin_in_voxels));
}

void VoxelProvider::_immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels) {
	immerge_block(buffer, Vector3i(origin_in_voxels));
}

void VoxelProvider::_bind_methods() {
	// Note: C++ inheriting classes don't need to re-bind these, because they are bindings that call the actual virtual methods

	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer", "origin_in_voxels"), &VoxelProvider::_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer", "origin_in_voxels"), &VoxelProvider::_immerge_block);
}
