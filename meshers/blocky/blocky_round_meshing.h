#ifndef BLOCKY_ROUND_MESHING_H
#define BLOCKY_ROUND_MESHING_H

#include "blocky_baked_library.h"
#include "blocky_round_mapping.h"
#include "voxel_mesher_blocky.h"

namespace zylann::voxel::blocky {

inline bool append_rounded_model(
		const uint64_t shape,
		const BakedModel &model,
		const BakedLibrary &library,
		const uint32_t visible_sides_mask,
		const Vector3f position,
		const Color modulate_color,
		const FixedArray<FixedArray<uint8_t, 4>, 6> &occlusion,
		const float occlusion_darkness,
		StdVector<VoxelMesherBlocky::Arrays> &out_arrays,
		StdVector<int> &index_offsets,
		VoxelMesher::Output::CollisionSurface *collision_surface,
		int &collision_index_offset
) {
	const round_geometry::IndexedMesh *mesh_ptr = library.rounding_geometry.get(shape);
	if (mesh_ptr == nullptr) {
		ZN_PRINT_ERROR("Missing baked rounded-cube geometry; retaining original geometry.");
		return false;
	}
	const round_geometry::IndexedMesh &mesh = *mesh_ptr;
	if (!mesh.valid) {
		ZN_PRINT_ERROR("Could not triangulate rounded cube; retaining original geometry.");
		return false;
	}
	FixedArray<RoundedFaceMap, 6> maps;
	if (!get_rounded_face_maps(model, maps))
		return false;
	const auto &vertices = library.rounding_geometry.get_vertices();
	const StdVector<RoundedVertexMapping> *mapping = nullptr;
	if (model.rounding_mapping_index < library.rounding_vertex_mappings.size()) {
		const auto &candidate = library.rounding_vertex_mappings[model.rounding_mapping_index];
		if (candidate.size() == vertices.size())
			mapping = &candidate;
	}
	static thread_local StdVector<int> remap;
	for (unsigned int surface_index = 0; surface_index < model.model.surface_count; ++surface_index) {
		const BakedModel::Surface &surface = model.model.surfaces[surface_index];
		VoxelMesherBlocky::Arrays &arrays = out_arrays[surface.material_id];
		int &index_offset = index_offsets[surface.material_id];
		const int first_vertex = index_offset;
		const int first_collision_vertex = collision_index_offset;
		const bool collision = collision_surface != nullptr && surface.collision_enabled;
		remap.assign(mesh.vertices.size(), -1);
		for (const round_geometry::Triangle &triangle : mesh.triangles) {
			if (triangle.flat_side >= 0 && (visible_sides_mask & (1 << rounded_cube_side(triangle.flat_side))) == 0) {
				continue;
			}
			if (maps[vertices[mesh.vertices[triangle.indices[0]]].face].surface != surface_index) {
				continue;
			}
			// Geometry is CCW; Godot uses clockwise front faces.
			for (int corner : { 0, 2, 1 }) {
				const int original_index = triangle.indices[corner];
				int &mapped_index = remap[original_index];
				if (mapped_index < 0) {
					const uint32_t vertex_id = mesh.vertices[original_index];
					const round_geometry::Vertex &vertex = vertices[vertex_id];
					const RoundedFaceMap &map = maps[vertex.face];
					mapped_index = index_offset++;
					const Vector3f p(vertex.p[0], vertex.p[1], vertex.p[2]);
					arrays.positions.push_back(p + position);
					arrays.normals.push_back(Vector3f(vertex.n[0], vertex.n[1], vertex.n[2]));
					// The slow path keeps unusual or absent mappings safe without mutating
					// shared data. Both paths use the original face's orientation/handedness.
					const RoundedVertexMapping mapped =
							mapping != nullptr ? (*mapping)[vertex_id] : map_rounded_vertex(*map.source, vertex);
					arrays.uvs.push_back(mapped.uv);
					if (!map.source->tangents.empty()) {
						for (float component : mapped.tangent)
							arrays.tangents.push_back(component);
					}
					float shade = 0;
					const unsigned int side = rounded_cube_side(vertex.face);
					for (unsigned int j = 0; j < 4; ++j) {
						const unsigned int cube_corner = Cube::g_side_corners[side][j];
						const float weight =
								math::max(0.f, 1.f - math::distance_squared(Cube::g_corner_position[cube_corner], p));
						shade = math::max(shade, occlusion_darkness * occlusion[side][j] * weight);
					}
					const float brightness = 1 - shade;
					arrays.colors.push_back(Color(brightness, brightness, brightness) * modulate_color);
					if (collision) {
						collision_surface->positions.push_back(p + position);
						++collision_index_offset;
					}
				}
				arrays.indices.push_back(mapped_index);
				if (collision) {
					collision_surface->indices.push_back(mapped_index - first_vertex + first_collision_vertex);
				}
			}
		}
	}
	return true;
}

} // namespace zylann::voxel::blocky
#endif
