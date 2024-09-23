#include "voxel_mesher_context.h"
#include "../../storage/voxel_buffer.h"
#include "../voxel_mesher.h"

namespace zylann::voxel {

int VoxelMesherContext::get_voxel(Vector3i pos, const int channel) const {
	ZN_ASSERT_RETURN_V(channel >= 0 && channel < static_cast<int>(VoxelBuffer::MAX_CHANNELS));
	return input->voxels.get_voxel(pos, channel);
}

float VoxelMesherContext::get_voxel_f(Vector3i pos, const int channel) const {
	ZN_ASSERT_RETURN_V(channel >= 0 && channel < static_cast<int>(VoxelBuffer::MAX_CHANNELS));
	return input->voxels.get_voxel_f(pos, channel);
}

Vector3i VoxelMesherContext::get_voxel_resolution_with_padding() const {
	return input->voxels.get_size();
}

int VoxelMesherContext::get_lod_index() const {
	return input->lod_index;
}

void VoxelMesherContext::add_surface_from_arrays(Array p_arrays, const int p_material_index) {
	ZN_ASSERT_RETURN(p_material_index >= 0 && p_material_index < 256);
	const uint16_t material_index = static_cast<uint16_t>(p_material_index);
	output->surfaces.push_back(VoxelMesherOutput::Surface{ p_arrays, material_index });
}

// This is not straightforward at the moment, lots of details can go wrong
// void VoxelMesherContext::add_mesher(Ref<VoxelMesher> mesher) {
// 	ZN_ASSERT_RETURN(mesher.is_valid());
// 	mesher->build(*output, *input);
// }

void VoxelMesherContext::_bind_methods() {
	using Self = VoxelMesherContext;

	ClassDB::bind_method(D_METHOD("get_voxel", "position", "channel"), &Self::get_voxel);
	ClassDB::bind_method(D_METHOD("get_voxel_f", "position", "channel"), &Self::get_voxel_f);
	ClassDB::bind_method(D_METHOD("get_voxel_resolution_with_padding"), &Self::get_voxel_resolution_with_padding);
	ClassDB::bind_method(D_METHOD("get_lod_index"), &Self::get_lod_index);
	ClassDB::bind_method(
			D_METHOD("add_surface_from_arrays", "arrays", "material_index"), &Self::add_surface_from_arrays
	);
}

} // namespace zylann::voxel
