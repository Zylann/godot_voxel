#include "voxel_generator.h"
#include "../constants/voxel_string_names.h"

VoxelGenerator::VoxelGenerator() {
}

VoxelGenerator::Result VoxelGenerator::generate_block(VoxelBlockRequest &input) {
	ERR_FAIL_COND_V(input.voxel_buffer.is_null(), Result());
	return Result();
}

int VoxelGenerator::get_used_channels_mask() const {
	return 0;
}

void VoxelGenerator::_b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	VoxelBlockRequest r = { out_buffer, Vector3i(origin_in_voxels), lod };
	generate_block(r);
}

void VoxelGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"),
			&VoxelGenerator::_b_generate_block);
}
