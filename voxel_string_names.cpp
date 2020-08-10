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
	emerge_block = StaticCString::create("emerge_block");
	immerge_block = StaticCString::create("immerge_block");
	generate_block = StaticCString::create("generate_block");
	get_used_channels_mask = StaticCString::create("get_used_channels_mask");
	block_loaded = StaticCString::create("block_loaded");
	block_unloaded = StaticCString::create("block_unloaded");

	u_transition_mask = StaticCString::create("u_transition_mask");
}
