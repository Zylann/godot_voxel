#include "voxel_stream.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/core/string.h"
#include "../util/string_funcs.h"

using namespace zylann::voxel;

VoxelStream::VoxelStream() {}

VoxelStream::~VoxelStream() {}

void VoxelStream::load_voxel_block(VoxelQueryData &query_data) {
	// Can be implemented in subclasses
	query_data.result = RESULT_BLOCK_NOT_FOUND;
}

void VoxelStream::save_voxel_block(VoxelQueryData &query_data) {
	// Can be implemented in subclasses
}

void VoxelStream::load_voxel_blocks(Span<VoxelQueryData> p_blocks) {
	// Default implementation. May matter for some stream types to optimize loading.
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		load_voxel_block(p_blocks[i]);
	}
}

void VoxelStream::save_voxel_blocks(Span<VoxelQueryData> p_blocks) {
	for (unsigned int i = 0; i < p_blocks.size(); ++i) {
		save_voxel_block(p_blocks[i]);
	}
}

bool VoxelStream::supports_instance_blocks() const {
	// Can be implemented in subclasses
	return false;
}

void VoxelStream::load_instance_blocks(Span<InstancesQueryData> out_blocks) {
	// Can be implemented in subclasses
	for (size_t i = 0; i < out_blocks.size(); ++i) {
		out_blocks[i].result = RESULT_BLOCK_NOT_FOUND;
	}
}

void VoxelStream::save_instance_blocks(Span<InstancesQueryData> p_blocks) {
	// Can be implemented in subclasses
}

void VoxelStream::load_all_blocks(FullLoadingResult &result) {
	ZN_PRINT_ERROR(format("{} does not support `load_all_blocks`", get_class()));
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
	return constants::DEFAULT_BLOCK_SIZE_PO2;
}

int VoxelStream::get_lod_count() const {
	return 1;
}

// Binding land

VoxelStream::ResultCode VoxelStream::_b_load_voxel_block(
		Ref<gd::VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND_V(lod < 0, RESULT_ERROR);
	ERR_FAIL_COND_V(out_buffer.is_null(), RESULT_ERROR);
	VoxelQueryData q{ out_buffer->get_buffer(), origin_in_voxels, lod, RESULT_ERROR };
	load_voxel_block(q);
	return q.result;
}

void VoxelStream::_b_save_voxel_block(Ref<gd::VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(lod < 0);
	ERR_FAIL_COND(buffer.is_null());
	VoxelQueryData q{ buffer->get_buffer(), origin_in_voxels, lod, RESULT_ERROR };
	save_voxel_block(q);
}

VoxelStream::ResultCode VoxelStream::_b_emerge_block(
		Ref<gd::VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod) {
	ERR_PRINT("VoxelStream.emerge_block is deprecated. Use `load_voxel_block` instead.");
	return _b_load_voxel_block(out_buffer, origin_in_voxels, lod);
}

void VoxelStream::_b_immerge_block(Ref<gd::VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod) {
	ERR_PRINT("VoxelStream.immerge_block is deprecated. Use `save_voxel_block` instead.");
	return _b_save_voxel_block(buffer, origin_in_voxels, lod);
}

int VoxelStream::_b_get_used_channels_mask() const {
	return get_used_channels_mask();
}

Vector3 VoxelStream::_b_get_block_size() const {
	return Vector3iUtil::create(1 << get_block_size_po2());
}

void VoxelStream::_bind_methods() {
	// Deprecated methods
	ClassDB::bind_method(
			D_METHOD("emerge_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelStream::_b_emerge_block);
	ClassDB::bind_method(
			D_METHOD("immerge_block", "buffer", "origin_in_voxels", "lod"), &VoxelStream::_b_immerge_block);

	ClassDB::bind_method(
			D_METHOD("load_voxel_block", "out_buffer", "origin_in_voxels", "lod"), &VoxelStream::_b_load_voxel_block);
	ClassDB::bind_method(
			D_METHOD("save_voxel_block", "buffer", "origin_in_voxels", "lod"), &VoxelStream::_b_save_voxel_block);
	ClassDB::bind_method(D_METHOD("get_used_channels_mask"), &VoxelStream::_b_get_used_channels_mask);

	ClassDB::bind_method(D_METHOD("set_save_generator_output", "enabled"), &VoxelStream::set_save_generator_output);
	ClassDB::bind_method(D_METHOD("get_save_generator_output"), &VoxelStream::get_save_generator_output);

	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelStream::_b_get_block_size);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_generator_output"), "set_save_generator_output",
			"get_save_generator_output");

	BIND_ENUM_CONSTANT(RESULT_ERROR);
	BIND_ENUM_CONSTANT(RESULT_BLOCK_FOUND);
	BIND_ENUM_CONSTANT(RESULT_BLOCK_NOT_FOUND);
}
