#include "voxel_generator_script.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"

VoxelGeneratorScript::VoxelGeneratorScript() {
}

void VoxelGeneratorScript::generate_block(VoxelBlockRequest &input) {
	ERR_FAIL_COND(input.voxel_buffer.is_null());
	try_call_script(this, VoxelStringNames::get_singleton()->_generate_block,
			input.voxel_buffer, input.origin_in_voxels.to_vec3(), input.lod, nullptr);
}

int VoxelGeneratorScript::get_used_channels_mask() const {
	Variant ret;
	if (try_call_script(this, VoxelStringNames::get_singleton()->_get_used_channels_mask, nullptr, 0, &ret)) {
		return ret;
	}
	return 0;
}

void VoxelGeneratorScript::_bind_methods() {
	BIND_VMETHOD(MethodInfo("_generate_block",
			PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
			PropertyInfo(Variant::VECTOR3, "origin_in_voxels"),
			PropertyInfo(Variant::INT, "lod")));

	BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}
