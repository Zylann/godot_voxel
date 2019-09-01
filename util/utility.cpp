#if TOOLS_ENABLED

#include "utility.h"
#include "../cube_tables.h"

namespace VoxelDebug {

Ref<Mesh> g_debug_box_mesh;

void create_debug_box_mesh() {
	PoolVector3Array positions;
	positions.resize(8);
	{
		PoolVector3Array::Write w = positions.write();
		for (int i = 0; i < positions.size(); ++i) {
			w[i] = Cube::g_corner_position[i];
		}
	}
	PoolIntArray indices;
	indices.resize(Cube::EDGE_COUNT * 2);
	{
		PoolIntArray::Write w = indices.write();
		int j = 0;
		for (int i = 0; i < Cube::EDGE_COUNT; ++i) {
			w[j++] = Cube::g_edge_corners[i][0];
			w[j++] = Cube::g_edge_corners[i][1];
		}
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = positions;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arrays);
	Ref<SpatialMaterial> mat;
	mat.instance();
	mat->set_albedo(Color(0, 1, 0));
	mat->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	mesh->surface_set_material(0, mat);
	g_debug_box_mesh = mesh;
}

void free_debug_box_mesh() {
	g_debug_box_mesh.unref();
}

Ref<Mesh> get_debug_box_mesh() {
	return g_debug_box_mesh;
}

} // namespace VoxelDebug

#endif
