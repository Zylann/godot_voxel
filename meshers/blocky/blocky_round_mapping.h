#ifndef BLOCKY_ROUND_MAPPING_H
#define BLOCKY_ROUND_MAPPING_H

#include "blocky_baked_library.h"
#include "blocky_round_geometry.h"

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

inline bool get_rounded_face_maps(const BakedModel &model, FixedArray<RoundedFaceMap, 6> &maps) {
	for (unsigned int side = 0; side < 6; ++side) {
		for (unsigned int surface = 0; surface < model.model.surface_count; ++surface) {
			const BakedModel::SideSurface &face = model.model.sides_surfaces[rounded_cube_side(side)][surface];
			if (!face.indices.empty()) {
				maps[side] = { &face, surface };
				break;
			}
		}
		if (maps[side].source == nullptr)
			return false;
	}
	return true;
}

inline RoundedVertexMapping map_rounded_vertex(
		const BakedModel::SideSurface &face,
		const round_geometry::Vertex &vertex
) {
	RoundedVertexMapping mapped{};
	round_geometry::V tangent{};
	float handedness = 1;
	sample_rounded_face(face, vertex, mapped.uv, tangent, handedness);
	if (!face.tangents.empty()) {
		using namespace round_geometry;
		tangent = norm(sub(tangent, mul(vertex.n, dot(tangent, vertex.n))));
	}
	for (unsigned int i = 0; i < 3; ++i)
		mapped.tangent[i] = tangent[i];
	mapped.tangent[3] = handedness;
	return mapped;
}

// Call only under the baked library's write lock, after updating geometry.
void bake_rounding_mappings(BakedLibrary &library);
} // namespace zylann::voxel::blocky
#endif
