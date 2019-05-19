#include "voxel_mesher.h"

Ref<Mesh> VoxelMesher::build_mesh(Ref<VoxelBuffer> voxels) {

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	Output output;
	build(output, **voxels, get_minimum_padding());

	if (output.surfaces.empty()) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instance();

	for (int i = 0; i < output.surfaces.size(); ++i) {
		mesh->add_surface_from_arrays(output.primitive_type, output.surfaces[i]);
	}

	return mesh;
}

void VoxelMesher::build(Output &output, const VoxelBuffer &voxels, int padding) {
}

int VoxelMesher::get_minimum_padding() const {
	return 0;
}

VoxelMesher *VoxelMesher::clone() {
	return nullptr;
}

void VoxelMesher::_bind_methods() {

	// Shortcut if you want to generate a mesh directly from a fixed grid of voxels.
	// Useful for testing the different meshers.
	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer"), &VoxelMesher::build_mesh);
}
