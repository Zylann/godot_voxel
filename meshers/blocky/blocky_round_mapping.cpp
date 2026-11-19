#include "blocky_round_mapping.h"
#include "../../util/containers/std_map.h"
#include <cstring>

namespace zylann::voxel::blocky {
namespace {
void append_float_key(StdVector<uint32_t> &key, float value) {
	uint32_t bits;
	static_assert(sizeof(bits) == sizeof(value));
	std::memcpy(&bits, &value, sizeof(bits));
	key.push_back(bits);
}

StdVector<uint32_t> face_mapping_key(const FixedArray<RoundedFaceMap, 6> &maps) {
	StdVector<uint32_t> key;
	for (const RoundedFaceMap &map : maps) {
		const BakedModel::SideSurface &face = *map.source;
		key.push_back(face.positions.size());
		for (const Vector3f p : face.positions) {
			append_float_key(key, p.x);
			append_float_key(key, p.y);
			append_float_key(key, p.z);
		}
		key.push_back(face.indices.size());
		for (int i : face.indices)
			key.push_back(i);
		key.push_back(face.uvs.size());
		for (const Vector2f uv : face.uvs) {
			append_float_key(key, uv.x);
			append_float_key(key, uv.y);
		}
		key.push_back(face.tangents.size());
		for (float t : face.tangents)
			append_float_key(key, t);
	}
	return key;
}
} // namespace

void bake_rounding_mappings(BakedLibrary &library) {
	StdVector<StdVector<RoundedVertexMapping>> mappings;
	StdMap<StdVector<uint32_t>, uint32_t> mapping_ids;
	const auto &vertices = library.rounding_geometry.get_vertices();
	for (BakedModel &model : library.models) {
		model.rounding_mapping_index = UINT32_MAX;
		if (!model.rounding_enabled || !model.is_full_cube || vertices.empty())
			continue;
		FixedArray<RoundedFaceMap, 6> maps;
		if (!get_rounded_face_maps(model, maps))
			continue;
		auto result = mapping_ids.emplace(face_mapping_key(maps), uint32_t(mappings.size()));
		if (result.second) {
			StdVector<RoundedVertexMapping> mapping;
			mapping.reserve(vertices.size());
			for (const round_geometry::Vertex &vertex : vertices) {
				mapping.push_back(map_rounded_vertex(*maps[vertex.face].source, vertex));
			}
			mappings.push_back(std::move(mapping));
		}
		model.rounding_mapping_index = result.first->second;
	}
	mappings.shrink_to_fit();
	library.rounding_vertex_mappings.swap(mappings);
}
} // namespace zylann::voxel::blocky
