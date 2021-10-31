#include "voxel_stream.h"
#include <core/script_language.h>

VoxelStream::VoxelStream() {
}

VoxelStream::~VoxelStream() {
}

VoxelStream::Result VoxelStream::emerge_block(VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) {
	// Can be implemented in subclasses
	return RESULT_BLOCK_NOT_FOUND;
}

void VoxelStream::immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) {
	// Can be implemented in subclasses
}

void VoxelStream::emerge_blocks(Span<VoxelBlockRequest> p_blocks, Vector<Result> &out_results) {
	// Default implementation. May matter for some stream types to optimize loading.
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		VoxelBlockRequest &r = p_blocks[i];
		const Result res = emerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
		out_results.push_back(res);
	}
}

void VoxelStream::immerge_blocks(Span<VoxelBlockRequest> p_blocks) {
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		const VoxelBlockRequest &r = p_blocks[i];
		immerge_block(r.voxel_buffer, r.origin_in_voxels, r.lod);
	}
}

bool VoxelStream::supports_instance_blocks() const {
	// Can be implemented in subclasses
	return false;
}

void VoxelStream::load_instance_blocks(
		Span<VoxelStreamInstanceDataRequest> out_blocks, Span<Result> out_results) {
	// Can be implemented in subclasses
	for (size_t i = 0; i < out_results.size(); ++i) {
		out_results[i] = RESULT_BLOCK_NOT_FOUND;
	}
}

void VoxelStream::save_instance_blocks(Span<VoxelStreamInstanceDataRequest> p_blocks) {
	// Can be implemented in subclasses
}

int VoxelStream::get_used_channels_mask() const {
	return 0;
}

void VoxelStream::set_save_generator_output(bool enabled) {
	RWLockWrite wlock(_parameters_lock);
	_parameters.save_generator_output = enabled;
}

bool VoxelStream::get_save_generator_output() const {
	RWLockRead rlock(_parameters_lock);
	return _parameters.save_generator_output;
}

int VoxelStream::get_block_size_po2() const {
	return VoxelConstants::DEFAULT_BLOCK_SIZE_PO2;
}

int VoxelStream::get_lod_count() const {
	return 1;
}

// Binding land

VoxelStream::Result VoxelStream::_b_emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND_V(lod < 0, RESULT_ERROR);
	ERR_FAIL_COND_V(out_buffer.is_null(), RESULT_ERROR);
	return emerge_block(out_buffer->get_buffer(), Vector3i::from_floored(origin_in_voxels), lod);
}

void VoxelStream::_b_immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	ERR_FAIL_COND(buffer.is_null());
	immerge_block(buffer->get_buffer(), Vector3i::from_floored(origin_in_voxels), lod);
}

int VoxelStream::_b_get_used_channels_mask() const {
	return get_used_channels_mask();
}

Vector3 VoxelStream::_b_get_block_size() const {
	return Vector3i(1 << get_block_size_po2()).to_vec3();
}

void VoxelStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("emerge_block", "out_buffer", "origin_in_voxels", "lod"),
			&VoxelStream::_b_emerge_block);
	ClassDB::bind_method(D_METHOD("immerge_block", "buffer", "origin_in_voxels", "lod"),
			&VoxelStream::_b_immerge_block);
	ClassDB::bind_method(D_METHOD("get_used_channels_mask"), &VoxelStream::_b_get_used_channels_mask);

	ClassDB::bind_method(D_METHOD("set_save_generator_output", "enabled"), &VoxelStream::set_save_generator_output);
	ClassDB::bind_method(D_METHOD("get_save_generator_output"), &VoxelStream::get_save_generator_output);

	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelStream::_b_get_block_size);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_generator_output"),
			"set_save_generator_output", "get_save_generator_output");

	BIND_ENUM_CONSTANT(RESULT_ERROR);
	BIND_ENUM_CONSTANT(RESULT_BLOCK_FOUND);
	BIND_ENUM_CONSTANT(RESULT_BLOCK_NOT_FOUND);
}
