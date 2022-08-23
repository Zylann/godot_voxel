#ifndef VOXEL_STRING_NAMES_H
#define VOXEL_STRING_NAMES_H

#include <core/string/string_name.h>

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

#ifdef DEBUG_ENABLED
	StringName _voxel_debug_vt_position;
#endif
};

} // namespace zylann::voxel

#endif // VOXEL_STRING_NAMES_H
