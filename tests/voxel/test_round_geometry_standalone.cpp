#ifdef VOXEL_ROUNDING_STANDALONE_TEST
#include "../../meshers/blocky/blocky_round_geometry.h"
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
using namespace zylann::voxel::blocky::round_geometry;
void check(bool condition, const char *message) {
	if (!condition) {
		throw std::runtime_error(message);
	}
}
void validate(uint64_t shape, const Settings &settings) {
	const Mesh mesh = generate(shape, settings);
	check(mesh.valid, "triangulation failed");
	check(!mesh.triangles.empty(), "empty mesh");
	std::map<std::pair<V, V>, std::pair<int, int>> edges;
	double volume = 0;
	for (const auto &vertex : mesh.vertices) {
		check(std::isfinite(dot(vertex.p, vertex.p)), "nonfinite position");
		check(std::abs(dot(vertex.n, vertex.n) - 1) < 1e-7, "nonunit normal");
		for (double x : vertex.p) {
			check(x >= -1e-8 && x <= 1 + 1e-8, "outside cube");
		}
	}
	for (const Triangle &triangle : mesh.triangles) {
		const auto &tri = triangle.indices;
		V a = mesh.vertices[tri[0]].p, b = mesh.vertices[tri[1]].p, c = mesh.vertices[tri[2]].p;
		check(dot(cross(sub(b, a), sub(c, a)), cross(sub(b, a), sub(c, a))) > 1e-22, "degenerate triangle");
		volume += dot(a, cross(b, c)) / 6.;
		for (int i = 0; i < 3; ++i) {
			V p = key(mesh.vertices[tri[i]].p), q = key(mesh.vertices[tri[(i + 1) % 3]].p);
			const bool forward = p < q;
			auto &counts = edges[forward ? std::make_pair(p, q) : std::make_pair(q, p)];
			++counts.first;
			counts.second += forward ? 1 : -1;
		}
	}
	check(volume > .5 && volume <= 1.000001, "invalid signed volume");
	for (const auto &entry : edges) {
		if (entry.second.first != 2 || entry.second.second != 0) {
			std::cerr << "bad edge " << entry.second.first << ": ";
			for (double x : entry.first.first) {
				std::cerr << x << ' ';
			}
			std::cerr << "-> ";
			for (double x : entry.first.second) {
				std::cerr << x << ' ';
			}
			std::cerr << '\n';
			check(false, "nonmanifold or mismatched edge");
		}
	}
}
int main() {
	uint64_t current = 0;
	Settings settings;
	size_t checked = 0;
	try {
		check(shape_key([](int, int, int) { return 0; }) == 4095, "isolated cube must round all 12 edges");
		check(shape_key([](int, int, int) { return 1; }) == 0, "interior cube must remain square");
		std::set<uint64_t> shapes{ 0, 4095 };
		std::mt19937 rng(89123);
		for (int iteration = 0; iteration < 2500; ++iteration) {
			std::array<int, 27> kinds{};
			for (int &kind : kinds) {
				kind = (rng() % 5) < 3 ? 0 : (rng() % 2) + 1;
			}
			shapes.insert(shape_key([&](int x, int y, int z) { return kinds[(x + 1) * 9 + (y + 1) * 3 + z + 1]; }));
		}
		// Every single edge and both end protections, in all orientations.
		for (unsigned int i = 0; i < 12; ++i) {
			for (uint64_t ends = 0; ends < 4; ++ends) {
				shapes.insert((uint64_t(1) << i) | (ends << (12 + 2 * i)));
			}
		}
		for (uint64_t shape : shapes) {
			current = shape;
			for (unsigned int mode = 0; mode < 4; ++mode) {
				for (double length : { .01, .05, .4, .6, .8, 1. }) {
					settings = { .1, length, mode };
					validate(shape, settings);
					++checked;
				}
			}
		}
		// Small and maximum supported radii are independent of taper length.
		for (double radius : { .001, .25 }) {
			for (uint64_t shape : { uint64_t(4095), uint64_t(1) | (uint64_t(3) << 12) }) {
				current = shape;
				settings = { radius, 1., 3 };
				validate(shape, settings);
				++checked;
			}
		}
		std::cout << "PASS " << checked << " closed meshes, " << shapes.size()
				  << " distinct shapes; all 12 edges, six lengths, four profiles.\n";
	} catch (const std::exception &e) {
		std::cerr << "FAIL shape=" << current << " radius=" << settings.radius << " length=" << settings.taper_distance
				  << " profile=" << settings.profile << ": " << e.what() << '\n';
		return 1;
	}
}

#endif
