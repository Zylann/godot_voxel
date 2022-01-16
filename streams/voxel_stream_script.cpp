#include "voxel_stream_script.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"

namespace zylann::voxel {

VoxelStream::Result VoxelStreamScript::emerge_block(
		VoxelBufferInternal &out_buffer, Vector3i origin_in_voxels, int lod) {
	Variant output;
	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instantiate();
	buffer_wrapper->get_buffer().copy_format(out_buffer);
	buffer_wrapper->get_buffer().create(out_buffer.get_size());

	//Result res;
	int res;
	if (GDVIRTUAL_CALL(_emerge_block, buffer_wrapper, origin_in_voxels, lod, res)) {
		if(res==RESULT_BLOCK_NOT_FOUND){
			ERR_FAIL_INDEX_V(res, _RESULT_COUNT, RESULT_ERROR);
			return Result(res);
		}
		if(res==RESULT_BLOCK_FOUND){
			buffer_wrapper->get_buffer().move_to(out_buffer);
			return Result(res);
		}
		
	} else {
		WARN_PRINT_ONCE("VoxelStreamScript::_emerge_block is unimplemented!");
	}

	
	return RESULT_ERROR;
}

void VoxelStreamScript::immerge_block(VoxelBufferInternal &buffer, Vector3i origin_in_voxels, int lod) {
	Ref<VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instantiate();
	buffer.duplicate_to(buffer_wrapper->get_buffer(), true);
	if (!GDVIRTUAL_CALL(_immerge_block, buffer_wrapper, origin_in_voxels, lod)) {
		WARN_PRINT_ONCE("VoxelStreamScript::_immerge_block is unimplemented!");
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
	GDVIRTUAL_BIND(_emerge_block, "out_buffer", "origin_in_voxels", "lod");
	GDVIRTUAL_BIND(_immerge_block, "buffer", "origin_in_voxels", "lod");
	GDVIRTUAL_BIND(_get_used_channels_mask);

	// BIND_VMETHOD(MethodInfo("_emerge_block",
	// 		PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
	// 		PropertyInfo(Variant::VECTOR3, "origin_in_voxels"), PropertyInfo(Variant::INT, "lod")));

	// BIND_VMETHOD(MethodInfo("_immerge_block",
	// 		PropertyInfo(Variant::OBJECT, "buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
	// 		PropertyInfo(Variant::VECTOR3, "origin_in_voxels"), PropertyInfo(Variant::INT, "lod")));

	// BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}

} // namespace zylann::voxel
