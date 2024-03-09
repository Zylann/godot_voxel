#ifndef VOX_IMPORT_FUNCS_H
#define VOX_IMPORT_FUNCS_H

#include "../../meshers/voxel_mesher.h"
#include "../../util/containers/std_vector.h"

// Some common functions to vox importers

namespace zylann::voxel::magica {

Ref<Mesh> build_mesh(const VoxelBuffer &voxels, VoxelMesher &mesher, StdVector<unsigned int> &surface_index_to_material,
		Ref<Image> &out_atlas, float p_scale, Vector3 p_offset);

} // namespace zylann::voxel::magica

#endif // VOX_IMPORT_FUNCS_H
