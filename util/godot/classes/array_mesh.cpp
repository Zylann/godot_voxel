#include "array_mesh.h"
#include "../funcs.h"

#include <map>
#include <unordered_map>

namespace zylann {

#ifdef TOOLS_ENABLED

Array generate_debug_seams_wireframe_surface(const ArrayMesh &src_mesh, int surface_index) {
	if (src_mesh.surface_get_primitive_type(surface_index) != Mesh::PRIMITIVE_TRIANGLES) {
		return Array();
	}
	Array src_surface = src_mesh.surface_get_arrays(surface_index);
	if (src_surface.is_empty()) {
		return Array();
	}
	PackedVector3Array src_positions = src_surface[Mesh::ARRAY_VERTEX];
	PackedVector3Array src_normals = src_surface[Mesh::ARRAY_NORMAL];
	PackedInt32Array src_indices = src_surface[Mesh::ARRAY_INDEX];
	if (src_indices.size() < 3) {
		return Array();
	}
	struct Dupe {
		int dst_index = 0;
		int count = 0;
	};

	// Using a map so we can have a comparator with floating error
	std::map<Vector3, Dupe> vertex_to_dupe;
	std::unordered_map<int, int> src_index_to_dst_index;
	std::vector<Vector3> dst_positions;
	{
		// const Vector3 *src_positions_read = src_positions.ptr();
		// const Vector3 *src_normals_read = src_normals.ptr();
		for (int i = 0; i < src_positions.size(); ++i) {
			const Vector3 pos = src_positions[i];
			auto dupe_it = vertex_to_dupe.find(pos);
			if (dupe_it == vertex_to_dupe.end()) {
				vertex_to_dupe.insert({ pos, Dupe() });
			} else {
				Dupe &dupe = dupe_it->second;
				if (dupe.count == 0) {
					dupe.dst_index = dst_positions.size();
					dst_positions.push_back(pos + src_normals[i] * 0.05);
				}
				++dupe.count;
				src_index_to_dst_index.insert({ i, dupe.dst_index });
			}
		}
	}

	std::vector<int32_t> dst_indices;
	{
		// PoolIntArray::Read r = src_indices.read();
		for (int i = 0; i < src_indices.size(); i += 3) {
			const int vi0 = src_indices[i];
			const int vi1 = src_indices[i + 1];
			const int vi2 = src_indices[i + 2];

			auto v0_it = src_index_to_dst_index.find(vi0);
			auto v1_it = src_index_to_dst_index.find(vi1);
			auto v2_it = src_index_to_dst_index.find(vi2);

			if (v0_it != src_index_to_dst_index.end() && v1_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v0_it->second);
				dst_indices.push_back(v1_it->second);
			}
			if (v1_it != src_index_to_dst_index.end() && v2_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v1_it->second);
				dst_indices.push_back(v2_it->second);
			}
			if (v2_it != src_index_to_dst_index.end() && v0_it != src_index_to_dst_index.end()) {
				dst_indices.push_back(v2_it->second);
				dst_indices.push_back(v0_it->second);
			}
		}
	}

	if (dst_indices.size() == 0) {
		return Array();
	}

	ERR_FAIL_COND_V(dst_indices.size() % 2 != 0, Array());
	ERR_FAIL_COND_V(dst_positions.size() < 2, Array());

	PackedVector3Array dst_positions_pv;
	PackedInt32Array dst_indices_pv;
	copy_to(dst_positions_pv, dst_positions);
	copy_to(dst_indices_pv, dst_indices);
	Array dst_surface;
	dst_surface.resize(Mesh::ARRAY_MAX);
	dst_surface[Mesh::ARRAY_VERTEX] = dst_positions_pv;
	dst_surface[Mesh::ARRAY_INDEX] = dst_indices_pv;
	return dst_surface;

	// Ref<ArrayMesh> wire_mesh;
	// wire_mesh.instance();
	// wire_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, dst_surface);

	// Ref<SpatialMaterial> line_material;
	// line_material->set_flag(SpatialMaterial::FLAG_UNSHADED, true);
	// line_material->set_albedo(Color(1.0, 0.0, 1.0));
	// wire_mesh->surface_set_material(0, line_material);

	// return wire_mesh;
}

#endif // TOOLS_ENABLED

} // namespace zylann
