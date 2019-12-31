#include "voxel_mesher.h"

Ref<Mesh> VoxelMesher::build_mesh(Ref<VoxelBuffer> voxels) {

	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	Output output;
	build(output, **voxels);

	if (output.surfaces.empty()) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instance();

	for (int i = 0; i < output.surfaces.size(); ++i) {
		mesh->add_surface_from_arrays(output.primitive_type, output.surfaces[i], Array(), output.compression_flags);
	}

	return mesh;
}

void VoxelMesher::build(Output &output, const VoxelBuffer &voxels) {

	ERR_PRINT("Not implemented");
}

int VoxelMesher::get_minimum_padding() const {
	return _minimum_padding;
}

int VoxelMesher::get_maximum_padding() const {
	return _maximum_padding;
}

void VoxelMesher::set_padding(int minimum, int maximum) {
	CRASH_COND(minimum < 0);
	CRASH_COND(maximum < 0);
	_minimum_padding = minimum;
	_maximum_padding = maximum;
}

VoxelMesher *VoxelMesher::clone() {
	return nullptr;
}

void VoxelMesher::_bind_methods() {

	// Shortcut if you want to generate a mesh directly from a fixed grid of voxels.
	// Useful for testing the different meshers.
	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer"), &VoxelMesher::build_mesh);
	ClassDB::bind_method(D_METHOD("get_minimum_padding"), &VoxelMesher::get_minimum_padding);
	ClassDB::bind_method(D_METHOD("get_maximum_padding"), &VoxelMesher::get_maximum_padding);
}
