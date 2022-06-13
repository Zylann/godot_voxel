#include "voxel_mesher.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/funcs.h"

namespace zylann::voxel {

Ref<Mesh> VoxelMesher::build_mesh(Ref<gd::VoxelBuffer> voxels, TypedArray<Material> materials) {
	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	Output output;
	Input input = { voxels->get_buffer(), 0 };
	build(output, input);

	if (output.surfaces.size() == 0) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	int surface_index = 0;
	for (unsigned int i = 0; i < output.surfaces.size(); ++i) {
		Output::Surface &surface = output.surfaces[i];
		Array &arrays = surface.arrays;

		if (arrays.is_empty()) {
			continue;
		}

		CRASH_COND(arrays.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(arrays)) {
			continue;
		}

		Ref<Material> material;
		if (int(i) < materials.size()) {
			material = materials[i];
		}
		if (material.is_null()) {
			material = get_material_by_index(i);
		}

		mesh->add_surface_from_arrays(output.primitive_type, arrays, Array(), Dictionary(), output.mesh_flags);
		mesh->surface_set_material(surface_index, material);
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

Ref<Material> VoxelMesher::get_material_by_index(unsigned int i) const {
	// May be implemented in some meshers
	return Ref<Material>();
}

void VoxelMesher::_bind_methods() {
	// Shortcut if you want to generate a mesh directly from a fixed grid of voxels.
	// Useful for testing the different meshers.
	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer", "materials"), &VoxelMesher::build_mesh);
	ClassDB::bind_method(D_METHOD("get_minimum_padding"), &VoxelMesher::get_minimum_padding);
	ClassDB::bind_method(D_METHOD("get_maximum_padding"), &VoxelMesher::get_maximum_padding);
}

} // namespace zylann::voxel
