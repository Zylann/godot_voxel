#ifndef VOXEL_STRING_NAMES_H
#define VOXEL_STRING_NAMES_H

#include <core/string_name.h>

class VoxelStringNames {
private:
	static VoxelStringNames *g_singleton;

public:
	inline static VoxelStringNames *get_singleton() {
		return g_singleton;
	}

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
};

#endif // VOXEL_STRING_NAMES_H
