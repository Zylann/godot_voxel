#ifndef VOXEL_FLOATING_CHUNKS_H
#define VOXEL_FLOATING_CHUNKS_H

#include "../meshers/voxel_mesher.h"
#include "../util/godot/core/array.h"
#include "../util/math/box3i.h"
#include "../util/math/transform_3d.h"

ZN_GODOT_FORWARD_DECLARE(class Node);

namespace zylann::voxel {

class VoxelTool;

Array separate_floating_chunks(
		VoxelTool &voxel_tool,
		Box3i world_box,
		Node *parent_node,
		Transform3D terrain_transform,
		Ref<VoxelMesher> mesher,
		Array materials
);

} // namespace zylann::voxel

#endif // VOXEL_FLOATING_CHUNKS_H
