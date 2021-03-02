#include "voxel_string_names.h"

VoxelStringNames *VoxelStringNames::g_singleton;

void VoxelStringNames::create_singleton() {
	CRASH_COND(g_singleton != nullptr);
	g_singleton = memnew(VoxelStringNames);
}

void VoxelStringNames::destroy_singleton() {
	CRASH_COND(g_singleton == nullptr);
	memdelete(g_singleton);
	g_singleton = nullptr;
}

VoxelStringNames::VoxelStringNames() {
	_emerge_block = StaticCString::create("_emerge_block");
	_immerge_block = StaticCString::create("_immerge_block");
	_generate_block = StaticCString::create("_generate_block");
	_get_used_channels_mask = StaticCString::create("_get_used_channels_mask");

	block_loaded = StaticCString::create("block_loaded");
	block_unloaded = StaticCString::create("block_unloaded");

	u_transition_mask = StaticCString::create("u_transition_mask");
	u_block_local_transform = StaticCString::create("u_block_local_transform");
	u_lod_fade = StaticCString::create("u_lod_fade");
}
