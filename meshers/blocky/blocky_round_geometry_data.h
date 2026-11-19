#ifndef BLOCKY_ROUND_GEOMETRY_DATA_H
#define BLOCKY_ROUND_GEOMETRY_DATA_H

#include <array>
#include <cstdint>

#ifdef VOXEL_ROUNDING_STANDALONE_TEST
// Standalone geometry checks do not link to Godot's memory allocator.
#include <map>
#include <vector>
namespace zylann {
template <typename T>
using StdVector = std::vector<T>;
template <typename K, typename V>
using StdMap = std::map<K, V>;
} // namespace zylann
#else
#include "../../util/containers/std_map.h"
#include "../../util/containers/std_vector.h"
#endif

namespace zylann::voxel::blocky::round_geometry {
using V = std::array<double, 3>;
struct Settings {
	double radius = .1;
	double taper_distance = .4;
	unsigned int profile = 0;
};
struct Vertex {
	V p, n, source_p;
	int face;
};
struct Triangle {
	std::array<int, 3> indices;
	int flat_side;
};
struct Mesh {
	StdVector<Vertex> vertices;
	StdVector<Triangle> triangles;
	bool valid = true;
};
} // namespace zylann::voxel::blocky::round_geometry
#endif
