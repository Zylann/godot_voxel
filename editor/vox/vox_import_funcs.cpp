#include "vox_import_funcs.h"
#include "../../util/godot/funcs.h"

namespace VoxImportUtils {

static void scale_surface(Array &surface, float scale) {
	PoolVector3Array positions = surface[Mesh::ARRAY_VERTEX];
	// Avoiding stupid CoW, assuming this array holds the only instance of this vector
	surface[Mesh::ARRAY_VERTEX] = PoolVector3Array();
	{
		PoolVector3Array::Write w = positions.write();
		for (int vertex_index = 0; vertex_index < positions.size(); ++vertex_index) {
			w[vertex_index] *= scale;
		}
	}
	surface[Mesh::ARRAY_VERTEX] = positions;
}

Ref<Mesh> build_mesh(VoxelBuffer &voxels, VoxelMesher &mesher,
		std::vector<unsigned int> &surface_index_to_material, Ref<Image> &out_atlas, float p_scale) {
	//
	VoxelMesher::Output output;
	VoxelMesher::Input input = { voxels, 0 };
	mesher.build(output, input);

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

		if (p_scale != 1.f) {
			scale_surface(surface, p_scale);
		}

		mesh->add_surface_from_arrays(output.primitive_type, surface, Array(), output.compression_flags);
		surface_index_to_material.push_back(i);
		++surface_index;
	}

	if (output.atlas_image.is_valid()) {
		out_atlas = output.atlas_image;
	}

	return mesh;
}

} // namespace VoxImportUtils
