#include "voxel_stream_script.h"
#include "../constants/voxel_string_names.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/check_ref_ownership.h"

namespace zylann::voxel {

void VoxelStreamScript::load_voxel_block(VoxelStream::VoxelQueryData &query_data) {
	Variant output;
	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<godot::VoxelBuffer> buffer_wrapper(memnew(
			godot::VoxelBuffer(static_cast<godot::VoxelBuffer::Allocator>(query_data.voxel_buffer.get_allocator()))));
	buffer_wrapper->get_buffer().copy_format(query_data.voxel_buffer);
	buffer_wrapper->get_buffer().create(query_data.voxel_buffer.get_size());

	query_data.result = RESULT_ERROR;

	ZN_GODOT_CHECK_REF_COUNT_DOES_NOT_CHANGE(buffer_wrapper);

	int res;
	if (GDVIRTUAL_CALL(_load_voxel_block, buffer_wrapper, query_data.position_in_blocks, query_data.lod_index, res)) {
		// Check if the return enum is valid
		ERR_FAIL_INDEX(res, _RESULT_COUNT);
		// If the block was found, grab its data from the script-facing object to our internal buffer
		if (res == RESULT_BLOCK_FOUND) {
			buffer_wrapper->get_buffer().move_to(query_data.voxel_buffer);
		}
		query_data.result = ResultCode(res);
	} else {
		// The function wasn't found or failed?
		WARN_PRINT_ONCE("VoxelStreamScript::_load_voxel_block is unimplemented!");
	}
}

void VoxelStreamScript::save_voxel_block(VoxelStream::VoxelQueryData &query_data) {
	// For now the callee can exceptionally take ownership of this wrapper, because we copy the data to it.
	Ref<godot::VoxelBuffer> buffer_wrapper(memnew(
			godot::VoxelBuffer(static_cast<godot::VoxelBuffer::Allocator>(query_data.voxel_buffer.get_allocator()))));
	query_data.voxel_buffer.copy_to(buffer_wrapper->get_buffer(), true);
	if (!GDVIRTUAL_CALL(_save_voxel_block, buffer_wrapper, query_data.position_in_blocks, query_data.lod_index)) {
		WARN_PRINT_ONCE("VoxelStreamScript::_save_voxel_block is unimplemented!");
	}
}

int VoxelStreamScript::get_used_channels_mask() const {
	int mask = 0;
	if (!GDVIRTUAL_CALL(_get_used_channels_mask, mask)) {
		WARN_PRINT_ONCE("VoxelStreamScript::_get_used_channels_mask is unimplemented!");
	}
	return mask;
}

void VoxelStreamScript::_bind_methods() {
	// TODO Test if GDVIRTUAL can print errors properly when GDScript fails inside a different thread.
	GDVIRTUAL_BIND(_load_voxel_block, "out_buffer", "position_in_blocks", "lod");
	GDVIRTUAL_BIND(_save_voxel_block, "buffer", "position_in_blocks", "lod");
	GDVIRTUAL_BIND(_get_used_channels_mask);
}

} // namespace zylann::voxel
