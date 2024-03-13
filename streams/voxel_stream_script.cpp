#include "voxel_stream_script.h"
#include "../constants/voxel_string_names.h"
#include "../storage/voxel_buffer_gd.h"

namespace zylann::voxel {

void VoxelStreamScript::load_voxel_block(VoxelStream::VoxelQueryData &query_data) {
	Variant output;
	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<godot::VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instantiate();
	buffer_wrapper->get_buffer().copy_format(query_data.voxel_buffer);
	buffer_wrapper->get_buffer().create(query_data.voxel_buffer.get_size());

	query_data.result = RESULT_ERROR;

	int res;
#if defined(ZN_GODOT)
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
#else
	ERR_PRINT_ONCE("VoxelStreamScript::load_voxel_block is not supported yet in GDExtension!");
#endif
}

void VoxelStreamScript::save_voxel_block(VoxelStream::VoxelQueryData &query_data) {
	Ref<godot::VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instantiate();
	query_data.voxel_buffer.copy_to(buffer_wrapper->get_buffer(), true);
#if defined(ZN_GODOT)
	if (!GDVIRTUAL_CALL(_save_voxel_block, buffer_wrapper, query_data.position_in_blocks, query_data.lod_index)) {
		WARN_PRINT_ONCE("VoxelStreamScript::_save_voxel_block is unimplemented!");
	}
#else
	ERR_PRINT_ONCE("VoxelStreamScript::save_voxel_block is not supported yet in GDExtension!");
#endif
}

int VoxelStreamScript::get_used_channels_mask() const {
	int mask = 0;
#if defined(ZN_GODOT)
	if (!GDVIRTUAL_CALL(_get_used_channels_mask, mask)) {
		WARN_PRINT_ONCE("VoxelStreamScript::_get_used_channels_mask is unimplemented!");
	}
#else
	ERR_PRINT_ONCE("VoxelStreamScript::get_used_channels_mask is not supported yet in GDExtension!");
#endif
	return mask;
}

void VoxelStreamScript::_bind_methods() {
#if defined(ZN_GODOT)
	// TODO Test if GDVIRTUAL can print errors properly when GDScript fails inside a different thread.
	GDVIRTUAL_BIND(_load_voxel_block, "out_buffer", "position_in_blocks", "lod");
	GDVIRTUAL_BIND(_save_voxel_block, "buffer", "position_in_blocks", "lod");
	GDVIRTUAL_BIND(_get_used_channels_mask);
#endif

	// BIND_VMETHOD(MethodInfo("_emerge_block",
	// 		PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
	// 		PropertyInfo(Variant::VECTOR3, "origin_in_voxels"), PropertyInfo(Variant::INT, "lod")));

	// BIND_VMETHOD(MethodInfo("_immerge_block",
	// 		PropertyInfo(Variant::OBJECT, "buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
	// 		PropertyInfo(Variant::VECTOR3, "origin_in_voxels"), PropertyInfo(Variant::INT, "lod")));

	// BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}

} // namespace zylann::voxel
