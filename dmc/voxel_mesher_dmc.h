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
	enum Mode {
		MODE_NORMAL,
		MODE_WIREFRAME,
		MODE_DEBUG_OCTREE,
		MODE_DEBUG_DUAL_GRID
	};

	Ref<ArrayMesh> build_mesh(Ref<VoxelBuffer> voxels, real_t geometric_error, Mode mode);

protected:
	static void _bind_methods();
};

VARIANT_ENUM_CAST(VoxelMesherDMC::Mode)

#endif // VOXEL_MESHER_DMC_H
