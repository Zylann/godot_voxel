#ifndef VOXEL_STRING_NAMES_H
#define VOXEL_STRING_NAMES_H

#include "core/string_name.h"

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

	StringName emerge_block;
	StringName immerge_block;
	StringName generate_block;
	StringName get_used_channels_mask;
	StringName block_loaded;
	StringName block_unloaded;

	StringName u_transition_mask;
};

#endif // VOXEL_STRING_NAMES_H
