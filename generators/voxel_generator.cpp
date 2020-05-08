#include "voxel_generator.h"
#include "../voxel_string_names.h"

VoxelGenerator::VoxelGenerator() {
}

void VoxelGenerator::generate_block(VoxelBlockRequest &input) {
	ERR_FAIL_COND(input.voxel_buffer.is_null());
	ScriptInstance *script = get_script_instance();

	if (script && script->has_method(VoxelStringNames::get_singleton()->generate_block)) {
		// Call script to generate buffer
		Variant arg1 = input.voxel_buffer;
		Variant arg2 = input.origin_in_voxels.to_vec3();
		Variant arg3 = input.lod;

		const Variant *args[3] = { &arg1, &arg2, &arg3 };
		Variant::CallError err;
		script->call(VoxelStringNames::get_singleton()->generate_block, args, 3, err);

		ERR_FAIL_COND_MSG(err.error != Variant::CallError::CALL_OK,
				"voxel_generator.cpp:generate_block gave an error: " + String::num(err.error) +
						", Argument: " + String::num(err.argument) +
						", Expected type: " + Variant::get_type_name(err.expected));

		// This had to be explicitely logged due to the usual GD debugger not working with threads
	}
}

//bool VoxelGenerator::is_thread_safe() const {
//	return false;
//}

//bool VoxelGenerator::is_cloneable() const {
//	return false;
//}

void VoxelGenerator::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	VoxelBlockRequest r = { out_buffer, Vector3i(origin_in_voxels), lod };
	generate_block(r);
}

void VoxelGenerator::_b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	VoxelBlockRequest r = { out_buffer, Vector3i(origin_in_voxels), lod };
	generate_block(r);
}

void VoxelGenerator::_bind_methods() {
	// Note: C++ inheriting classes don't need to re-bind these, because they are bindings that call the actual virtual methods

	ClassDB::bind_method(D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelGenerator::_b_generate_block);
}
