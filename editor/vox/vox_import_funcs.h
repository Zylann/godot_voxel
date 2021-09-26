#ifndef VOX_IMPORT_FUNCS_H
#define VOX_IMPORT_FUNCS_H

#include "../../meshers/voxel_mesher.h"

// Some common functions to vox importers

namespace VoxImportUtils {

Ref<Mesh> build_mesh(const VoxelBufferInternal &voxels, VoxelMesher &mesher,
		std::vector<unsigned int> &surface_index_to_material, Ref<Image> &out_atlas, float p_scale, Vector3 p_offset);

} // namespace VoxImportUtils

#endif // VOX_IMPORT_FUNCS_H
