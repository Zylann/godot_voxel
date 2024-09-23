#ifndef VOXEL_MESHER_INPUT_H
#define VOXEL_MESHER_INPUT_H

#include "../util/math/vector3i.h"
#include <cstdint>

namespace zylann::voxel {

class VoxelBuffer;
class VoxelGenerator;

// Due to annoying requirement that GDVIRTUAL needs the full definition of parameters, I had to separate this struct
// outside the class it was originally in.

struct VoxelMesherInput {
	// Voxels to be used as the primary source of data.
	const VoxelBuffer &voxels;
	// When using LOD, some meshers can use the generator and edited voxels to affine results.
	// If not provided, the mesher will only use `voxels`.
	VoxelGenerator *generator = nullptr;
	// Origin of the block is required when doing deep sampling.
	Vector3i origin_in_voxels;
	// LOD index. 0 means highest detail. 1 means half detail etc.
	uint8_t lod_index = 0;
	// If true, collision information is required.
	// Sometimes it doesn't change anything as the rendering mesh can be used as collider,
	// but in other setups it can be different and will be returned in `collision_surface`.
	bool collision_hint = false;
	// If true, the mesher is told that the mesh will be used in a context with variable level of detail.
	// For example, transition meshes will or will not be generated based on this (overriding mesher settings).
	bool lod_hint = false;
	// If true, the mesher can collect some extra information which can be useful to speed up detail texture
	// baking. Depends on the mesher.
	bool detail_texture_hint = false;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_OUTPUT_H
