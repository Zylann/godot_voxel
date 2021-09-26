#include "voxel_mesher.h"
#include "../storage/voxel_buffer.h"
#include "../util/godot/funcs.h"

Ref<Mesh> VoxelMesher::build_mesh(Ref<VoxelBuffer> voxels, Array materials) {
	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	Output output;
	Input input = { voxels->get_buffer(), 0 };
	build(output, input);

	if (output.surfaces.empty()) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instance();

	int surface_index = 0;
	for (int i = 0; i < output.surfaces.size(); ++i) {
		Array surface = output.surfaces[i];
		if (surface.empty()) {
			continue;
		}

		CRASH_COND(surface.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(surface)) {
			continue;
		}

		mesh->add_surface_from_arrays(output.primitive_type, surface, Array(), output.compression_flags);
		if (i < materials.size()) {
			mesh->surface_set_material(surface_index, materials[i]);
		}
		++surface_index;
	}

	return mesh;
}

void VoxelMesher::build(Output &output, const Input &input) {
	ERR_PRINT("Not implemented");
}

unsigned int VoxelMesher::get_minimum_padding() const {
	return _minimum_padding;
}

unsigned int VoxelMesher::get_maximum_padding() const {
	return _maximum_padding;
}

void VoxelMesher::set_padding(int minimum, int maximum) {
	CRASH_COND(minimum < 0);
	CRASH_COND(maximum < 0);
	_minimum_padding = minimum;
	_maximum_padding = maximum;
}

void VoxelMesher::_bind_methods() {
	// Shortcut if you want to generate a mesh directly from a fixed grid of voxels.
	// Useful for testing the different meshers.
	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer", "materials"), &VoxelMesher::build_mesh);
	ClassDB::bind_method(D_METHOD("get_minimum_padding"), &VoxelMesher::get_minimum_padding);
	ClassDB::bind_method(D_METHOD("get_maximum_padding"), &VoxelMesher::get_maximum_padding);
}
