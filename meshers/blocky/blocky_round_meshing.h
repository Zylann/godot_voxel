#ifndef BLOCKY_ROUND_MESHING_H
#define BLOCKY_ROUND_MESHING_H

#include "blocky_baked_library.h"
#include "blocky_round_geometry.h"
#include "voxel_mesher_blocky.h"

namespace zylann::voxel::blocky {

inline unsigned int rounded_cube_side(int side) {
	// The engine's historic X side indices are reversed.
	return side < 2 ? 1 - side : side;
}

struct RoundedFaceMap {
	const BakedModel::SideSurface *source = nullptr;
	unsigned int surface = 0;
};

// Interpolate within the original triangles, preserving imported UV layouts and
// their diagonal. No atlas dimensions or game-specific materials are assumed.
inline void sample_rounded_face(
		const BakedModel::SideSurface &face,
		const round_geometry::Vertex &vertex,
		Vector2f &uv,
		round_geometry::V &tangent,
		float &handedness
) {
	const int axis = vertex.face / 2, u = (axis + 1) % 3, v = (axis + 2) % 3;
	for (unsigned int triangle = 0; triangle < 2; ++triangle) {
		const int ia = face.indices[triangle * 3], ib = face.indices[triangle * 3 + 1],
				  ic = face.indices[triangle * 3 + 2];
		const Vector3f a = face.positions[ia], b = face.positions[ib], c = face.positions[ic];
		const double px = vertex.source_p[u] - a[u], py = vertex.source_p[v] - a[v];
		const double bx = b[u] - a[u], by = b[v] - a[v], cx = c[u] - a[u], cy = c[v] - a[v];
		const double det = bx * cy - by * cx;
		const float wb = (px * cy - py * cx) / det, wc = (bx * py - by * px) / det, wa = 1 - wb - wc;
		if (triangle == 0 && (wa < -0.00001f || wb < -0.00001f || wc < -0.00001f)) {
			continue;
		}
		uv = face.uvs[ia] * wa + face.uvs[ib] * wb + face.uvs[ic] * wc;
		if (!face.tangents.empty()) {
			for (unsigned int i = 0; i < 3; ++i) {
				tangent[i] = face.tangents[4 * ia + i] * wa + face.tangents[4 * ib + i] * wb +
						face.tangents[4 * ic + i] * wc;
			}
			handedness = face.tangents[4 * ia + 3];
		}
		return;
	}
}

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
	const round_geometry::Mesh *mesh_ptr = library.rounding_geometry.get(shape);
	if (mesh_ptr == nullptr) {
		ZN_PRINT_ERROR("Missing baked rounded-cube geometry; retaining original geometry.");
		return false;
	}
	const round_geometry::Mesh &mesh = *mesh_ptr;
	if (!mesh.valid) {
		ZN_PRINT_ERROR("Could not triangulate rounded cube; retaining original geometry.");
		return false;
	}
	FixedArray<RoundedFaceMap, 6> maps;
	for (unsigned int side = 0; side < 6; ++side) {
		for (unsigned int surface = 0; surface < model.model.surface_count; ++surface) {
			const BakedModel::SideSurface &face = model.model.sides_surfaces[rounded_cube_side(side)][surface];
			if (!face.indices.empty()) {
				maps[side] = { &face, surface };
				break;
			}
		}
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
			if (maps[mesh.vertices[triangle.indices[0]].face].surface != surface_index) {
				continue;
			}
			// Geometry is CCW; Godot uses clockwise front faces.
			for (int corner : { 0, 2, 1 }) {
				const int original_index = triangle.indices[corner];
				int &mapped_index = remap[original_index];
				if (mapped_index < 0) {
					const round_geometry::Vertex &vertex = mesh.vertices[original_index];
					const RoundedFaceMap &map = maps[vertex.face];
					mapped_index = index_offset++;
					const Vector3f p(vertex.p[0], vertex.p[1], vertex.p[2]);
					arrays.positions.push_back(p + position);
					arrays.normals.push_back(Vector3f(vertex.n[0], vertex.n[1], vertex.n[2]));
					Vector2f uv;
					round_geometry::V tangent{};
					float handedness = 1;
					sample_rounded_face(*map.source, vertex, uv, tangent, handedness);
					arrays.uvs.push_back(uv);
					if (!map.source->tangents.empty()) {
						using namespace round_geometry;
						tangent = norm(sub(tangent, mul(vertex.n, dot(tangent, vertex.n))));
						for (double component : tangent) {
							arrays.tangents.push_back(component);
						}
						arrays.tangents.push_back(handedness);
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
