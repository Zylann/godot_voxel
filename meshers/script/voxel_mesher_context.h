#ifndef VOXEL_MESHER_CONTEXT_H
#define VOXEL_MESHER_CONTEXT_H

#include "../../util/godot/classes/object.h"
#include "../voxel_mesher_input.h"
#include "../voxel_mesher_output.h"

namespace zylann::voxel {

// Data and functions provided to script when meshing one chunk of voxels.
// Must not be created or stored by scripts.
class VoxelMesherContext : public Object {
	GDCLASS(VoxelMesherContext, Object)
public:
	const VoxelMesherInput *input = nullptr;
	VoxelMesherOutput *output = nullptr;

	// TODO Maybe we need a VoxelBufferReadOnly?
	// Ref<VoxelBuffer> voxels_wrapper;
	// Ref<VoxelBuffer> get_voxels();

	// Voxels access (read-only)
	int get_voxel(Vector3i pos, const int channel) const;
	float get_voxel_f(Vector3i pos, const int channel) const;
	Vector3i get_voxel_resolution_with_padding() const;
	int get_lod_index() const;

	// Output
	void add_surface_from_arrays(Array p_arrays, const int p_material_index);
	// void add_mesher(Ref<VoxelMesher> mesher);

private:
	static void _bind_methods();
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_CONTEXT_H
