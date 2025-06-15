#ifndef VOXEL_FLOATING_CHUNKS_H
#define VOXEL_FLOATING_CHUNKS_H

#include "../storage/voxel_buffer.h"
#include "../util/godot/core/array.h"
#include "../util/godot/core/transform_3d.h"
#include "../util/math/box3i.h"

ZN_GODOT_FORWARD_DECLARE(class Node);

namespace zylann::voxel {

class VoxelTool;
class VoxelMesher;

Array separate_floating_chunks_to_rigidbodies(
		VoxelTool &voxel_tool,
		Box3i world_box,
		const VoxelBuffer::ChannelId main_channel,
		Node *parent_node,
		Transform3D terrain_transform,
		VoxelMesher &mesher,
		Array materials
);

} // namespace zylann::voxel

#endif // VOXEL_FLOATING_CHUNKS_H
