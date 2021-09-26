#include "voxel_generator_script.h"
#include "../constants/voxel_string_names.h"
#include "../util/godot/funcs.h"

VoxelGeneratorScript::VoxelGeneratorScript() {
}

VoxelGenerator::Result VoxelGeneratorScript::generate_block(VoxelBlockRequest &input) {
	Result result;
	Variant ret;
	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instance();
	buffer_wrapper->get_buffer().copy_format(input.voxel_buffer);
	buffer_wrapper->get_buffer().create(input.voxel_buffer.get_size());
	try_call_script(this, VoxelStringNames::get_singleton()->_generate_block,
			buffer_wrapper, input.origin_in_voxels.to_vec3(), input.lod, &ret);
	// The wrapper is discarded
	buffer_wrapper->get_buffer().move_to(input.voxel_buffer);

	// We may expose this to scripts the day it actually gets used
	// if (ret.get_type() == Variant::DICTIONARY) {
	// 	Dictionary d = ret;
	// 	result.max_lod_hint = d.get("max_lod_hint", false);
	// }

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
	BIND_VMETHOD(MethodInfo("_generate_block",
			PropertyInfo(Variant::OBJECT, "out_buffer", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "VoxelBuffer"),
			PropertyInfo(Variant::VECTOR3, "origin_in_voxels"),
			PropertyInfo(Variant::INT, "lod")));

	BIND_VMETHOD(MethodInfo(Variant::INT, "_get_used_channels_mask"));
}
