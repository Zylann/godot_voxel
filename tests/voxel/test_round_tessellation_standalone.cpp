#ifdef VOXEL_ROUNDING_STANDALONE_TEST
// Standalone: compile with blocky_round_geometry_table.cpp.
#include "../../meshers/blocky/blocky_round_geometry.h"
#include "../../meshers/blocky/blocky_round_geometry_table.h"
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
using namespace zylann::voxel::blocky::round_geometry;
using Segment = std::pair<V, V>;
struct Boundary {
	std::set<Segment> segments;
	std::map<V, std::set<V>> normals;
};
V qpoint(V p, double scale) {
	for (auto &x : p)
		x = std::round(x * scale) / scale;
	return p;
}
Boundary boundary(const Mesh &mesh, int face) {
	const int axis = face / 2;
	const double value = (face & 1) ? 1 : 0;
	std::map<Segment, int> counts;
	Boundary out;
	for (const auto &triangle : mesh.triangles) {
		if (triangle.flat_side == face) {
			for (int j = 0; j < 3; ++j) {
				V a = mesh.vertices[triangle.indices[j]].p, b = mesh.vertices[triangle.indices[(j + 1) % 3]].p;
				a[axis] = b[axis] = 0;
				a = qpoint(a, 1e8);
				b = qpoint(b, 1e8);
				if (a == b)
					continue;
				++counts[a < b ? std::make_pair(a, b) : std::make_pair(b, a)];
			}
		} else {
			for (int i : triangle.indices) {
				auto &vertex = mesh.vertices[i];
				if (std::abs(vertex.p[axis] - value) > 1e-8)
					continue;
				V p = vertex.p;
				p[axis] = 0;
				p = qpoint(p, 1e8);
				const int a = (axis + 1) % 3, b = (axis + 2) % 3;
				// A collapsed square endpoint intentionally has sector normals
				// from its taper. Test continuing nonzero bevel arcs instead.
				if ((p[a] < 1e-8 || p[a] > 1 - 1e-8) && (p[b] < 1e-8 || p[b] > 1 - 1e-8))
					continue;
				out.normals[p].insert(qpoint(vertex.n, 1e6));
			}
		}
	}
	for (const auto &[segment, count] : counts)
		if (count == 1)
			out.segments.insert(segment);
	return out;
}
void validate_adjacent_cubes() {
	std::mt19937 random(991927);
	size_t checks = 0, position_failures = 0, normal_failures = 0;
	for (double radius : { double(.001f), double(.15f), double(.25f) })
		for (double length : { double(.05f), double(.4f), 1. })
			for (unsigned int profile = 0; profile < 4; ++profile)
				for (int axis = 0; axis < 3; ++axis)
					for (int iteration = 0; iteration < 100; ++iteration) {
						std::array<int, 64> cells{};
						for (auto &c : cells)
							c = iteration < 3 ? iteration : ((random() % 5) < 3 ? 0 : 1 + random() % 2);
						std::array<int, 3> left{ 1, 1, 1 }, right = left;
						++right[axis];
						auto index = [](std::array<int, 3> p) { return p[0] * 16 + p[1] * 4 + p[2]; };
						cells[index(left)] = cells[index(right)] = 1;
						auto shape = [&](std::array<int, 3> center) {
							return shape_key([&](int x, int y, int z) {
								return cells[index({ center[0] + x, center[1] + y, center[2] + z })];
							});
						};
						const auto a = shape(left), b = shape(right);
						const auto ba = boundary(generate(a, { radius, length, profile }), 2 * axis + 1),
								   bb = boundary(generate(b, { radius, length, profile }), 2 * axis);
						if (ba.segments != bb.segments) {
							if (position_failures < 8)
								std::cout << "POSITION_FAIL " << a << ' ' << b << " axis=" << axis
										  << " radius=" << radius << " length=" << length << " profile=" << profile
										  << '\n';
							++position_failures;
						}
						if (ba.normals != bb.normals) {
							if (normal_failures < 8)
								std::cout << "NORMAL_FAIL " << a << ' ' << b << " axis=" << axis << " radius=" << radius
										  << " length=" << length << " profile=" << profile << '\n';
							++normal_failures;
						}
						++checks;
					}
	std::cout << "JOINS pairs=" << checks << " position_failures=" << position_failures
			  << " normal_failures=" << normal_failures << '\n';
	if (position_failures || normal_failures)
		throw std::runtime_error("Adjacent rounded contours or normals differ");
}

void validate_closed_mesh(const Mesh &mesh) {
	if (!mesh.valid || mesh.triangles.empty())
		throw std::runtime_error("Invalid rounded mesh");
	std::map<Segment, std::pair<int, int>> edges;
	double volume = 0;
	for (const auto &vertex : mesh.vertices) {
		if (!std::isfinite(dot(vertex.n, vertex.n)) || std::abs(dot(vertex.n, vertex.n) - 1) > 1e-7)
			throw std::runtime_error("Nonunit rounded normal");
		for (double p : vertex.p)
			if (!std::isfinite(p) || p < -1e-8 || p > 1 + 1e-8)
				throw std::runtime_error("Rounded vertex outside unit cube");
	}
	for (const auto &triangle : mesh.triangles) {
		const V a = mesh.vertices[triangle.indices[0]].p, b = mesh.vertices[triangle.indices[1]].p,
				c = mesh.vertices[triangle.indices[2]].p;
		const V area = cross(sub(b, a), sub(c, a));
		if (dot(area, area) < 1e-22)
			throw std::runtime_error("Degenerate rounded triangle");
		volume += dot(a, cross(b, c)) / 6;
		for (int j = 0; j < 3; ++j) {
			const V p = qpoint(mesh.vertices[triangle.indices[j]].p, 1e8),
					q = qpoint(mesh.vertices[triangle.indices[(j + 1) % 3]].p, 1e8);
			const bool forward = p < q;
			auto &entry = edges[forward ? std::make_pair(p, q) : std::make_pair(q, p)];
			++entry.first;
			entry.second += forward ? 1 : -1;
		}
	}
	if (volume <= .5 || volume > 1.000001)
		throw std::runtime_error("Invalid rounded volume or winding");
	for (const auto &entry : edges)
		if (entry.second.first != 2 || entry.second.second != 0)
			throw std::runtime_error("Open or nonmanifold rounded surface at 1e-8 voxel precision");
}

int main() {
	uint64_t shape = 0;
	Settings settings;
	size_t checked = 0;
	try {
		auto shapes = generate_reachable_shape_keys();
		shapes.insert(shapes.begin(), 0);
		for (double radius : { double(.001f), double(.1f), double(.15f), double(.25f) })
			for (double length : { double(.01f), double(.05f), double(.4f), double(.6f), double(.8f), 1. })
				for (unsigned int profile = 0; profile < 4; ++profile)
					for (uint64_t key : shapes) {
						shape = key;
						settings = { radius, length, profile };
						validate_closed_mesh(generate(shape, settings));
						++checked;
					}
		const Mesh isolated = generate(4095, { double(.15f), 1., 0 });
		if (isolated.triangles.size() > 108)
			throw std::runtime_error("Isolated rounded cube exceeds triangle budget");
		// Preserve nondegenerate projection into each original model face.
		// Rotated/mirrored/imported UV and tangent mapping is checked separately.
		for (uint64_t key : shapes) {
			const Mesh mesh = generate(key, { double(.15f), 1., 0 });
			for (const auto &triangle : mesh.triangles) {
				const auto &a = mesh.vertices[triangle.indices[0]], &b = mesh.vertices[triangle.indices[1]],
						   &c = mesh.vertices[triangle.indices[2]];
				const V uv_area = cross(sub(b.source_p, a.source_p), sub(c.source_p, a.source_p));
				if (dot(uv_area, uv_area) < 1e-28)
					throw std::runtime_error("Degenerate source-face projection");
				if (dot(cross(sub(b.p, a.p), sub(c.p, a.p)), add(add(a.n, b.n), c.n)) < -1e-10)
					throw std::runtime_error("Triangle winding opposes normals");
			}
		}
		validate_adjacent_cubes();
		std::cout << "TESSELLATION_PASS closed_meshes=" << checked << " shapes=" << shapes.size()
				  << " configurations=96 isolated_triangles=" << isolated.triangles.size() << '\n';
	} catch (const std::exception &e) {
		std::cerr << "TESSELLATION_FAIL shape=" << shape << " radius=" << settings.radius
				  << " length=" << settings.taper_distance << " profile=" << settings.profile << ": " << e.what()
				  << '\n';
		return 1;
	}
}

#endif
