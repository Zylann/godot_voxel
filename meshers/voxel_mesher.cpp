#include "voxel_mesher.h"
#include "../constants/voxel_string_names.h"
#include "../generators/voxel_generator.h"
#include "../storage/voxel_buffer_gd.h"
#include "../util/godot/funcs.h"

namespace zylann::voxel {

template <typename T>
T try_get(const Dictionary &d, String key) {
	const Variant *v = d.getptr(key);
	if (v == nullptr) {
		return T();
	}
	return *v;
}

Ref<Mesh> VoxelMesher::build_mesh(
		Ref<gd::VoxelBuffer> voxels, TypedArray<Material> materials, Dictionary additional_data) {
	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());

	Output output;
	Input input = { voxels->get_buffer() };

	if (additional_data.size() > 0) {
		Ref<VoxelGenerator> generator = try_get<Ref<VoxelGenerator>>(additional_data, "generator");
		input.generator = generator.ptr();
		input.origin_in_voxels = try_get<Vector3i>(additional_data, "origin_in_voxels");
	}

	build(output, input);

	if (output.surfaces.size() == 0) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	int gd_surface_index = 0;
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
		if (int(surface.material_index) < materials.size()) {
			material = materials[surface.material_index];
		}
		if (material.is_null()) {
			material = get_material_by_index(surface.material_index);
		}

		mesh->add_surface_from_arrays(output.primitive_type, arrays, Array(), Dictionary(), output.mesh_flags);
		mesh->surface_set_material(gd_surface_index, material);
		++gd_surface_index;
	}

	if (output.normalmap_atlas.is_valid()) {
		// That should be in return value, but for now I just want this for testing with GDScript, so it gotta go
		// somewhere
		mesh->set_meta(VoxelStringNames::get_singleton().voxel_normalmap_atlas, output.normalmap_atlas);
		mesh->set_meta(VoxelStringNames::get_singleton().voxel_normalmap_lookup, output.normalmap_lookup);
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
	// TODO Have an object type to specify input
	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer", "materials", "additional_data"),
			&VoxelMesher::build_mesh, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("get_minimum_padding"), &VoxelMesher::get_minimum_padding);
	ClassDB::bind_method(D_METHOD("get_maximum_padding"), &VoxelMesher::get_maximum_padding);
}

} // namespace zylann::voxel
