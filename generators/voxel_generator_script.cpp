#include "voxel_generator_script.h"
#include "../constants/voxel_string_names.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/funcs.h"

namespace zylann::voxel {

VoxelGeneratorScript::VoxelGeneratorScript() {}

VoxelGenerator::Result VoxelGeneratorScript::generate_block(VoxelGenerator::VoxelQueryData &input) {
	Result result;

	// Create a temporary wrapper so Godot can pass it to scripts
	Ref<gd::VoxelBuffer> buffer_wrapper;
	buffer_wrapper.instantiate();
	buffer_wrapper->get_buffer().copy_format(input.voxel_buffer);
	buffer_wrapper->get_buffer().create(input.voxel_buffer.get_size());

#if defined(ZN_GODOT)
	if (!GDVIRTUAL_CALL(_generate_block, buffer_wrapper, input.origin_in_voxels, input.lod)) {
		WARN_PRINT_ONCE("VoxelGeneratorScript::_generate_block is unimplemented!");
	}
#else
	ERR_PRINT_ONCE("VoxelGeneratorScript::_generate_block is not supported yet in GDExtension!");
#endif

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
	int mask = 0;
#if defined(ZN_GODOT)
	if (!GDVIRTUAL_CALL(_get_used_channels_mask, mask)) {
		WARN_PRINT_ONCE("VoxelGeneratorScript::_get_used_channels_mask is unimplemented!");
	}
#else
	ERR_PRINT_ONCE("VoxelGeneratorScript::_get_used_channels_mask is not supported yet in GDExtension!");
#endif
	return mask;
}

void VoxelGeneratorScript::_bind_methods() {
#if defined(ZN_GODOT)
	// TODO Test if GDVIRTUAL can print errors properly when GDScript fails inside a different thread.
	GDVIRTUAL_BIND(_generate_block, "out_buffer", "origin_in_voxels", "lod");
	GDVIRTUAL_BIND(_get_used_channels_mask);
#endif
}

} // namespace zylann::voxel
