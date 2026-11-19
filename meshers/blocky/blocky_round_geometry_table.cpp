#include "blocky_round_geometry_table.h"
#include "blocky_round_geometry.h"
#include <cstring>

namespace zylann::voxel::blocky::round_geometry {
namespace {
unsigned int cell_index(const std::array<int, 3> &p) {
	return (p[0] + 1) * 9 + (p[1] + 1) * 3 + p[2] + 1;
}
} // namespace

StdVector<uint64_t> generate_reachable_shape_keys() {
	StdVector<uint64_t> keys;
	// Six face neighbors each have three classes: air, rounded cube, other solid.
	// A diagonal is relevant only between an air face and a rounded-cube face:
	// it can protect the endpoint of an exposed edge continuing into that cube.
	// For any other pair, the edge is absent or its endpoint is already decided.
	for (unsigned int combination = 0; combination < 729; ++combination) {
		std::array<int, 27> cells{};
		std::array<int, 6> faces{};
		unsigned int n = combination;
		for (int axis = 0; axis < 3; ++axis) {
			for (int sign : { -1, 1 }) {
				std::array<int, 3> p{};
				p[axis] = sign;
				faces[side(axis, sign)] = n % 3;
				cells[cell_index(p)] = n % 3;
				n /= 3;
			}
		}
		std::array<unsigned int, 12> variable_cells{};
		unsigned int variable_count = 0;
		for (unsigned int i = 0; i < 12; ++i) {
			const Edge e = edge(i);
			const int a = faces[side(e.a, e.sa)], b = faces[side(e.b, e.sb)];
			if (!((a == 0 && b == 1) || (a == 1 && b == 0)))
				continue;
			std::array<int, 3> p{};
			p[e.a] = e.sa;
			p[e.b] = e.sb;
			variable_cells[variable_count++] = cell_index(p);
		}
		for (unsigned int mask = 0; mask < (1u << variable_count); ++mask) {
			for (unsigned int i = 0; i < variable_count; ++i)
				cells[variable_cells[i]] = (mask >> i) & 1;
			const uint64_t shape = shape_key([&](int x, int y, int z) { return cells[cell_index({ x, y, z })]; });
			if (shape != 0)
				keys.push_back(shape);
		}
	}
	std::sort(keys.begin(), keys.end());
	keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
	return keys;
}

bool GeometryTable::update(const Settings &settings, bool enabled) {
	if (!enabled || settings.radius < .001) {
		const bool changed = !_entries.empty();
		StdVector<Entry>().swap(_entries);
		StdVector<Vertex>().swap(_vertices);
		_settings = settings;
		return changed;
	}
	if (!_entries.empty() && settings.radius == _settings.radius &&
		settings.taper_distance == _settings.taper_distance && settings.profile == _settings.profile)
		return false;

	const StdVector<uint64_t> keys = generate_reachable_shape_keys();
	StdVector<Entry> entries;
	entries.reserve(keys.size());
	StdVector<Vertex> vertices;
	StdMap<std::array<uint64_t, 10>, uint32_t> vertex_ids;
	for (uint64_t shape : keys) {
		Mesh generated = generate(shape, settings);
		IndexedMesh mesh;
		mesh.valid = generated.valid;
		mesh.vertices.reserve(generated.vertices.size());
		for (const Vertex &v : generated.vertices) {
			const std::array<double, 10> values{ v.p[0], v.p[1],		v.p[2],		   v.n[0],		  v.n[1],
												 v.n[2], v.source_p[0], v.source_p[1], v.source_p[2], double(v.face) };
			std::array<uint64_t, 10> key;
			// Preserve signed zeros as well as every other bit; no quantization or
			// approximate welding is allowed across UV and normal seams.
			static_assert(sizeof(key) == sizeof(values));
			std::memcpy(key.data(), values.data(), sizeof(key));
			const auto result = vertex_ids.emplace(key, uint32_t(vertices.size()));
			if (result.second)
				vertices.push_back(v);
			mesh.vertices.push_back(result.first->second);
		}
		mesh.triangles = std::move(generated.triangles);
		mesh.triangles.shrink_to_fit();
		entries.push_back({ shape, std::move(mesh) });
	}
	vertices.shrink_to_fit();
	_entries.swap(entries);
	_vertices.swap(vertices);
	_settings = settings;
	return true;
}

const IndexedMesh *GeometryTable::get(uint64_t shape) const {
	const auto it = std::lower_bound(_entries.begin(), _entries.end(), shape, [](const Entry &entry, uint64_t key) {
		return entry.shape < key;
	});
	return it != _entries.end() && it->shape == shape ? &it->mesh : nullptr;
}

size_t GeometryTable::get_storage_bytes() const {
	size_t bytes = _entries.capacity() * sizeof(Entry) + _vertices.capacity() * sizeof(Vertex);
	for (const Entry &entry : _entries) {
		bytes += entry.mesh.vertices.capacity() * sizeof(uint32_t);
		bytes += entry.mesh.triangles.capacity() * sizeof(Triangle);
	}
	return bytes;
}
} // namespace zylann::voxel::blocky::round_geometry
