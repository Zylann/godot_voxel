#include "voxel_string_names.h"
#include "../util/errors.h"
#include "../util/memory.h"

namespace zylann::voxel {

VoxelStringNames *VoxelStringNames::g_singleton;

void VoxelStringNames::create_singleton() {
	ZN_ASSERT(g_singleton == nullptr);
	g_singleton = ZN_NEW(VoxelStringNames);
}

void VoxelStringNames::destroy_singleton() {
	ZN_ASSERT(g_singleton != nullptr);
	ZN_DELETE(g_singleton);
	g_singleton = nullptr;
}

const VoxelStringNames &VoxelStringNames::get_singleton() {
	ZN_ASSERT(g_singleton != nullptr);
	return *g_singleton;
}

VoxelStringNames::VoxelStringNames() {
	_emerge_block = StringName("_emerge_block");
	_immerge_block = StringName("_immerge_block");
	_generate_block = StringName("_generate_block");
	_get_used_channels_mask = StringName("_get_used_channels_mask");

	block_loaded = StringName("block_loaded");
	block_unloaded = StringName("block_unloaded");

	mesh_block_entered = StringName("mesh_block_entered");
	mesh_block_exited = StringName("mesh_block_exited");

	store_colors_in_texture = StringName("store_colors_in_texture");
	scale = StringName("scale");
	enable_baked_lighting = StringName("enable_baked_lighting");
	pivot_mode = StringName("pivot_mode");

	u_transition_mask = StringName("u_transition_mask");
	u_block_local_transform = StringName("u_block_local_transform");
	u_lod_fade = StringName("u_lod_fade");

	voxel_normalmap_atlas = StringName("voxel_normalmap_atlas");
	voxel_normalmap_lookup = StringName("voxel_normalmap_lookup");

	u_voxel_normalmap_atlas = StringName("u_voxel_normalmap_atlas");
	u_voxel_cell_lookup = StringName("u_voxel_cell_lookup");
	u_voxel_cell_size = StringName("u_voxel_cell_size");
	u_voxel_block_size = StringName("u_voxel_block_size");
	u_voxel_virtual_texture_fade = StringName("u_voxel_virtual_texture_fade");
	u_voxel_virtual_texture_tile_size = StringName("u_voxel_virtual_texture_tile_size");
	u_voxel_virtual_texture_offset_scale = StringName("u_voxel_virtual_texture_offset_scale");

#ifdef DEBUG_ENABLED
	_voxel_debug_vt_position = StringName("_voxel_debug_vt_position");
#endif

	changed = StringName("changed");
	frame_post_draw = StringName("frame_post_draw");

#ifdef TOOLS_ENABLED
	Add = StringName("Add");
	Remove = StringName("Remove");
	EditorIcons = StringName("EditorIcons");
	Pin = StringName("Pin");
	ExternalLink = StringName("ExternalLink");
#endif

	_rpc_receive_blocks = StringName("_rpc_receive_blocks");
	_rpc_receive_area = StringName("_rpc_receive_area");
}

} // namespace zylann::voxel
