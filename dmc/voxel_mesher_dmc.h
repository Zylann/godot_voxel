#ifndef VOXEL_MESHER_DMC_H
#define VOXEL_MESHER_DMC_H

#include "../voxel_buffer.h"
#include "scene/resources/mesh.h"

namespace dmc {

Ref<ArrayMesh> polygonize(const VoxelBuffer &voxels, float geometric_error);
}

class VoxelMesherDMC : public Reference {
	GDCLASS(VoxelMesherDMC, Reference)
public:
	Ref<ArrayMesh> build_mesh(Ref<VoxelBuffer> voxels, real_t geometric_error);

protected:
	static void _bind_methods();
};

#endif // VOXEL_MESHER_DMC_H
