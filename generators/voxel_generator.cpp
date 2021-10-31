#include "voxel_generator.h"
#include "../constants/voxel_string_names.h"

VoxelGenerator::VoxelGenerator() {
}

VoxelGenerator::Result VoxelGenerator::generate_block(VoxelBlockRequest &input) {
	return Result();
}

int VoxelGenerator::get_used_channels_mask() const {
	return 0;
}

VoxelSingleValue VoxelGenerator::generate_single(Vector3i pos, unsigned int channel) {
	// Default slow implementation
	VoxelBufferInternal buffer;
	buffer.create(1, 1, 1);
	VoxelBlockRequest r{ buffer, pos, 0 };
	generate_block(r);
	VoxelSingleValue v;
	if (channel == VoxelBufferInternal::CHANNEL_SDF) {
		v.f = buffer.get_voxel_f(0, 0, 0, channel);
	} else {
		v.i = buffer.get_voxel(0, 0, 0, channel);
	}
	return v;
}

void VoxelGenerator::_b_generate_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	ERR_FAIL_COND(out_buffer.is_null());
	VoxelBlockRequest r = { out_buffer->get_buffer(), Vector3i::from_floored(origin_in_voxels), lod };
	generate_block(r);
}

void VoxelGenerator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("generate_block", "out_buffer", "origin_in_voxels", "lod"),
			&VoxelGenerator::_b_generate_block);
}
