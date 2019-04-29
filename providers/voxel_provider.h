#ifndef VOXEL_PROVIDER_H
#define VOXEL_PROVIDER_H

#include "../voxel_buffer.h"
#include <core/resource.h>

class VoxelProvider : public Resource {
	GDCLASS(VoxelProvider, Resource)
public:
	virtual void emerge_block(Ref<VoxelBuffer> out_buffer, Vector3i origin_in_voxels, int lod);
	virtual void immerge_block(Ref<VoxelBuffer> buffer, Vector3i origin_in_voxels, int lod);

protected:
	static void _bind_methods();

	void _emerge_block(Ref<VoxelBuffer> out_buffer, Vector3 origin_in_voxels, int lod);
	void _immerge_block(Ref<VoxelBuffer> buffer, Vector3 origin_in_voxels, int lod);
};

#endif // VOXEL_PROVIDER_H
