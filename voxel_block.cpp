#include "voxel_block.h"

MeshInstance *VoxelBlock::get_mesh_instance(const Node &root) {
	if (mesh_instance_path.is_empty())
		return NULL;
	Node *n = root.get_node(mesh_instance_path);
	if (n == NULL)
		return NULL;
	return n->cast_to<MeshInstance>();
}

// Helper
VoxelBlock *VoxelBlock::create(Vector3i bpos, Ref<VoxelBuffer> buffer, unsigned int size) {
	const int bs = size;
	ERR_FAIL_COND_V(buffer.is_null(), NULL);
	ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), NULL);

	VoxelBlock *block = memnew(VoxelBlock);
	block->pos = bpos;

	block->voxels = buffer;
	//block->map = &map;
	return block;
}

VoxelBlock::VoxelBlock()
	: voxels(NULL) {
}

