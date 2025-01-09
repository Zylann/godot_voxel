#include "voxel_blocky_texture_atlas_tile_inspector.h"
#include "../../util/godot/classes/editor_spin_slider.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/label.h"

namespace zylann::voxel {

VoxelBlockyTextureAtlasTileInspector::VoxelBlockyTextureAtlasTileInspector() {}

void VoxelBlockyTextureAtlasTileInspector::set_tile(Ref<VoxelBlockyTextureAtlas> atlas, const int tile_id) {
	_atlas = atlas;
	_tile_id = tile_id;
}

} // namespace zylann::voxel
