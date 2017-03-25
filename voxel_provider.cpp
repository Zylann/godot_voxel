#include "voxel_provider.h"
#include "voxel_map.h"


void VoxelProvider::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i block_pos) {
	ERR_FAIL_COND(out_buffer.is_null());
	ScriptInstance * script = get_script_instance();
	if(script) {
		// Call script to generate buffer
		Variant arg1 = out_buffer;
		Variant arg2 = block_pos.to_vec3();
		const Variant * args[2] = { &arg1, &arg2 };
		//Variant::CallError err; // wut
		script->call_multilevel("emerge_block", args, 2);
	}
}

void VoxelProvider::immerge_block(Ref<VoxelBuffer> buffer, Vector3i block_pos) {
	ERR_FAIL_COND(buffer.is_null());
	ScriptInstance * script = get_script_instance();
	if(script) {
		// Call script to save buffer
		Variant arg1 = buffer;
		Variant arg2 = block_pos.to_vec3();
		const Variant * args[2] = { &arg1, &arg2 };
		//Variant::CallError err; // wut
		script->call_multilevel("immerge_block", args, 2);
	}
}

void VoxelProvider::_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 block_pos) {
	emerge_block(out_buffer, Vector3i(block_pos));
}

void VoxelProvider::_immerge_block(Ref<VoxelBuffer> buffer, Vector3 block_pos) {
	immerge_block(buffer, Vector3i(block_pos));
}

void VoxelProvider::_bind_methods() {

	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer:VoxelBuffer", "block_pos:Vector3"), &VoxelProvider::_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer:VoxelBuffer", "block_pos:Vector3"), &VoxelProvider::_immerge_block);

}


