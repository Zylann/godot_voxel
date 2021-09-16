#include "voxel_generator_script.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"

VoxelGeneratorScript::VoxelGeneratorScript() {
}

VoxelGenerator::Result VoxelGeneratorScript::generate_block(VoxelBlockRequest &input) {
	Result result;
	ERR_FAIL_COND_V(input.voxel_buffer.is_null(), result);
	Variant ret;
	try_call_script(this, VoxelStringNames::get_singleton()->_generate_block,
			input.voxel_buffer, input.origin_in_voxels.to_vec3(), input.lod, &ret);
	if (ret.get_type() == Variant::DICTIONARY) {
		Dictionary d = ret;
		result.max_lod_hint = d.get("max_lod_hint", false);
	}
	return result;
}

int VoxelGeneratorScript::get_used_channels_mask() const {
	Variant ret;
	if (try_call_script(this, VoxelStringNames::get_singleton()->_get_used_channels_mask, nullptr, 0, &ret)) {
		return ret;
	}
	return 0;
}

void VoxelGeneratorScript::_bind_methods() {
	// The dictionary return is optional
	BIND_VMETHOD(MethodInfo(Variant::DICTIONARY, "_generate_block",
			PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
			PropertyInfo(Variant::VECTOR3, "origin_in_voxels"),
			PropertyInfo(Variant::INT, "lod")));

	BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}
