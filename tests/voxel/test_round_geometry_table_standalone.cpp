#ifdef VOXEL_ROUNDING_STANDALONE_TEST
#include "../../meshers/blocky/blocky_round_geometry.h"
#include "../../meshers/blocky/blocky_round_geometry_table.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
using namespace zylann::voxel::blocky::round_geometry;
using Clock = std::chrono::steady_clock;
static void check(bool b, const char *message) {
	if (!b)
		throw std::runtime_error(message);
}
static double elapsed(Clock::time_point t) {
	return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
static bool same(const IndexedMesh &a, const Mesh &b, const GeometryTable &table) {
	if (a.valid != b.valid || a.vertices.size() != b.vertices.size() || a.triangles.size() != b.triangles.size())
		return false;
	for (size_t i = 0; i < a.vertices.size(); ++i) {
		const auto &x = table.get_vertices()[a.vertices[i]], &y = b.vertices[i];
		if (std::memcmp(x.p.data(), y.p.data(), sizeof(V)) != 0 ||
			std::memcmp(x.n.data(), y.n.data(), sizeof(V)) != 0 ||
			std::memcmp(x.source_p.data(), y.source_p.data(), sizeof(V)) != 0 || x.face != y.face)
			return false;
	}
	for (size_t i = 0; i < a.triangles.size(); ++i)
		if (a.triangles[i].indices != b.triangles[i].indices || a.triangles[i].flat_side != b.triangles[i].flat_side)
			return false;
	return true;
}
int main() {
	try {
		std::set<uint64_t> expected;
		for (unsigned faces = 0; faces < 729; ++faces) {
			std::array<int, 27> cells{};
			unsigned n = faces;
			for (int axis = 0; axis < 3; ++axis)
				for (int sign : { -1, 1 }) {
					std::array<int, 3> p{};
					p[axis] = sign;
					cells[(p[0] + 1) * 9 + (p[1] + 1) * 3 + p[2] + 1] = n % 3;
					n /= 3;
				}
			for (unsigned mask = 0; mask < 4096; ++mask) {
				for (unsigned e = 0; e < 12; ++e) {
					const Edge d = edge(e);
					std::array<int, 3> p{};
					p[d.a] = d.sa;
					p[d.b] = d.sb;
					cells[(p[0] + 1) * 9 + (p[1] + 1) * 3 + p[2] + 1] = (mask >> e) & 1;
				}
				expected.insert(shape_key([&](int x, int y, int z) {
					check(std::abs(x) + std::abs(y) + std::abs(z) <= 2, "unexpected neighborhood dependency");
					return cells[(x + 1) * 9 + (y + 1) * 3 + z + 1];
				}));
			}
		}
		expected.erase(0);
		auto started = Clock::now();
		const auto keys = generate_reachable_shape_keys();
		const double enum_ms = elapsed(started);
		check(expected == std::set<uint64_t>(keys.begin(), keys.end()),
			  "compact enumeration differs from exhaustive neighborhoods");
		check(keys.size() == 857, "reachable shape count changed");
		std::cout << "ENUM_PASS configurations=2985984 nonzero=" << keys.size() << " compact_ms=" << enum_ms << '\n';
		GeometryTable table;
		size_t comparisons = 0;
		for (double radius : { double(.001f), double(.1f), double(.15f), double(.25f) })
			for (double length : { double(.05f), double(.4f), 1. })
				for (unsigned profile = 0; profile < 4; ++profile) {
					const Settings settings{ radius, length, profile };
					started = Clock::now();
					check(table.update(settings, true), "changed settings not rebuilt");
					const double bake_ms = elapsed(started);
					check(table.get_entries().size() == 857, "incomplete baked table");
					const IndexedMesh *saved = table.get(4095);
					check(saved != nullptr, "isolated cube missing");
					check(!table.update(settings, true) && table.get(4095) == saved,
						  "identical settings rebuilt geometry");
					for (uint64_t key : keys) {
						check(supports_connected_edges(key), "reachable key uses fallback generator");
						const IndexedMesh *m = table.get(key);
						check(m != nullptr && same(*m, generate(key, settings), table),
							  "baked geometry differs from direct generator");
						++comparisons;
					}
					check(table.get(0) == nullptr && table.get(UINT64_MAX) == nullptr, "unknown key returned geometry");
					if ((radius == double(.1f) && length == double(.4f) && profile == 0) ||
						(radius == double(.15f) && length == 1 && profile == 0) ||
						(radius == double(.1f) && length == double(.05f) && profile == 3))
						std::cout << "TABLE radius=" << radius << " length=" << length << " profile=" << profile
								  << " bake_ms=" << bake_ms << " retained_bytes=" << table.get_storage_bytes() << '\n';
				}
		const Settings current{ double(.15f), 1, 0 };
		table.update(current, true);
		size_t referenced_vertices = 0;
		for (const auto &entry : table.get_entries())
			referenced_vertices += entry.mesh.vertices.size();
		check(table.get_vertices().size() < referenced_vertices, "vertex records were not shared across shapes");
		std::array<std::thread, 4> threads;
		std::atomic<size_t> reads{ 0 };
		for (auto &thread : threads)
			thread = std::thread([&] {
				for (unsigned pass = 0; pass < 200; ++pass)
					for (uint64_t key : keys) {
						check(table.get(key) != nullptr, "concurrent lookup missing");
						++reads;
					}
			});
		for (auto &thread : threads)
			thread.join();
		check(reads == 4 * 200 * 857, "concurrent read count mismatch");
		started = Clock::now();
		volatile size_t sink = 0;
		for (unsigned pass = 0; pass < 1000; ++pass)
			for (uint64_t key : keys)
				sink += table.get(key)->triangles.size();
		std::cout << "LOOKUP_857000_MS " << elapsed(started) << " checksum=" << sink << '\n';
		check(table.update(current, false) && table.get_entries().empty() && table.get_storage_bytes() == 0,
			  "disabled table retained storage");
		check(!table.update(current, false), "disabled table rebuilt");
		check(!table.update({ 0, 1, 0 }, true) && table.get_entries().empty(), "zero-radius table generated");
		check(!table.update({ .0005, 1, 0 }, true) && table.get_entries().empty(), "subminimum radius table generated");
		check(table.update(current, true), "re-enable did not rebuild");
		std::cout << "PASS exact_geometry_comparisons=" << comparisons << " concurrent_reads=" << reads
				  << " configurations=48\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "FAIL " << e.what() << '\n';
		return 1;
	}
}
#endif
