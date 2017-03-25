#ifndef VOXEL_PROVIDER_H
#define VOXEL_PROVIDER_H

#include "reference.h"
#include "voxel_buffer.h"


class VoxelProvider : public Reference {
	GDCLASS(VoxelProvider, Reference)
public:
	virtual void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i block_pos);
	virtual void immerge_block(Ref<VoxelBuffer> buffer, Vector3i block_pos);

protected:
	static void _bind_methods();

	void _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 block_pos);
	void _immerge_block(Ref<VoxelBuffer> buffer, Vector3 block_pos);
};


#endif // VOXEL_PROVIDER_H
