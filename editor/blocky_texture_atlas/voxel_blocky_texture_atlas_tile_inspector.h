#ifndef ZN_VOXEL_BLOCKY_TEXTURE_ATLAS_TILE_INSPECTOR_H
#define ZN_VOXEL_BLOCKY_TEXTURE_ATLAS_TILE_INSPECTOR_H

#include "../../meshers/blocky/voxel_blocky_texture_atlas.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/macros.h"

namespace zylann::voxel {

class VoxelBlockyTextureAtlasTileInspector : public VBoxContainer {
	GDCLASS(VoxelBlockyTextureAtlasTileInspector, VBoxContainer)
public:
	VoxelBlockyTextureAtlasTileInspector();

	void set_tile(Ref<VoxelBlockyTextureAtlas> atlas, const int tile_id);

private:
	static void _bind_methods() {}

	Ref<VoxelBlockyTextureAtlas> _atlas;
	int _tile_id = -1;
};

}; // namespace zylann::voxel

#endif // ZN_VOXEL_BLOCKY_TEXTURE_ATLAS_TILE_INSPECTOR_H
