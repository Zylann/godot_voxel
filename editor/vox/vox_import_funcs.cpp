#include "vox_import_funcs.h"
#include "../../util/godot/classes/array_mesh.h"

namespace zylann {

namespace voxel::magica {

Ref<Mesh> build_mesh(const VoxelBufferInternal &voxels, VoxelMesher &mesher,
		std::vector<unsigned int> &surface_index_to_material, Ref<Image> &out_atlas, float p_scale, Vector3 p_offset) {
	//
	VoxelMesher::Output output;
	VoxelMesher::Input input = { voxels, nullptr, nullptr, Vector3i(), 0, false };
	mesher.build(output, input);

	if (output.surfaces.size() == 0) {
		return Ref<ArrayMesh>();
	}

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	for (unsigned int i = 0; i < output.surfaces.size(); ++i) {
		VoxelMesher::Output::Surface &surface = output.surfaces[i];
		Array arrays = surface.arrays;

		if (arrays.is_empty()) {
			continue;
		}

		CRASH_COND(arrays.size() != Mesh::ARRAY_MAX);
		if (!is_surface_triangulated(arrays)) {
			continue;
		}

		if (p_scale != 1.f) {
			scale_surface(arrays, p_scale);
		}

		if (p_offset != Vector3()) {
			offset_surface(arrays, p_offset);
		}

		mesh->add_surface_from_arrays(output.primitive_type, arrays, Array(), Dictionary(), output.mesh_flags);
		surface_index_to_material.push_back(i);
	}

	if (output.atlas_image.is_valid()) {
		out_atlas = output.atlas_image;
	}

	return mesh;
}

} // namespace voxel::magica
} // namespace zylann
