#ifndef VOXEL_STRING_NAMES_H
#define VOXEL_STRING_NAMES_H

#include "../util/godot/core/string_name.h"

namespace zylann::voxel {

class VoxelStringNames {
private:
	static VoxelStringNames *g_singleton;

public:
	static const VoxelStringNames &get_singleton();
	static void create_singleton();
	static void destroy_singleton();

	VoxelStringNames();

	StringName _emerge_block;
	StringName _immerge_block;
	StringName _generate_block;
	StringName _get_used_channels_mask;

	StringName block_loaded;
	StringName block_unloaded;

	StringName mesh_block_entered;
	StringName mesh_block_exited;

	StringName store_colors_in_texture;
	StringName scale;
	StringName enable_baked_lighting;
	StringName pivot_mode;

	StringName u_transition_mask;
	StringName u_block_local_transform;
	StringName u_lod_fade;

	StringName voxel_normalmap_atlas;
	StringName voxel_normalmap_lookup;

	StringName u_voxel_normalmap_atlas;
	StringName u_voxel_cell_lookup;
	StringName u_voxel_cell_size;
	StringName u_voxel_block_size;
	StringName u_voxel_virtual_texture_fade;
	StringName u_voxel_virtual_texture_tile_size;
	StringName u_voxel_virtual_texture_offset_scale;

#ifdef DEBUG_ENABLED
	StringName _voxel_debug_vt_position;
#endif

	// These are usually in CoreStringNames, but when compiling as a GDExtension, we don't have access to them
	StringName changed;
	StringName frame_post_draw;

#ifdef TOOLS_ENABLED
	StringName Add;
	StringName Remove;
	StringName EditorIcons;
	StringName Pin;
	StringName ExternalLink;
#endif

	StringName _rpc_receive_blocks;
	StringName _rpc_receive_area;
};

} // namespace zylann::voxel

#endif // VOXEL_STRING_NAMES_H
