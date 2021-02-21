#include "voxel_stream_script.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"

VoxelStream::Result VoxelStreamScript::emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND_V(out_buffer.is_null(), RESULT_ERROR);
	Variant output;
	if (try_call_script(this, VoxelStringNames::get_singleton()->_emerge_block,
				out_buffer, origin_in_voxels.to_vec3(), lod, &output)) {
		int res = output;
		ERR_FAIL_INDEX_V(res, _RESULT_COUNT, RESULT_ERROR);
		return static_cast<Result>(res);
	}
	return RESULT_ERROR;
}

void VoxelStreamScript::immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod) {
	ERR_FAIL_COND(buffer.is_null());
	try_call_script(this, VoxelStringNames::get_singleton()->_immerge_block,
			buffer, origin_in_voxels.to_vec3(), lod, nullptr);
}

int VoxelStreamScript::get_used_channels_mask() const {
	Variant ret;
	if (try_call_script(this, VoxelStringNames::get_singleton()->_get_used_channels_mask, nullptr, 0, &ret)) {
		return ret;
	}
	return 0;
}

void VoxelStreamScript::_bind_methods() {
	BIND_VMETHOD(MethodInfo("_emerge_block",
			PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
			PropertyInfo(Variant::VECTOR3, "origin_in_voxels"),
			PropertyInfo(Variant::INT, "lod")));

	BIND_VMETHOD(MethodInfo("_immerge_block",
			PropertyInfo(Variant::OBJECT, "buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
			PropertyInfo(Variant::VECTOR3, "origin_in_voxels"),
			PropertyInfo(Variant::INT, "lod")));

	BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}
