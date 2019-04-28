#ifndef CUBE_TABLES_H
#define CUBE_TABLES_H

#include "math/vector3i.h"
#include <core/math/vector3.h>

namespace Cube {

// Index convention used in some lookup tables
enum Side {
	SIDE_LEFT = 0,
	SIDE_RIGHT,
	SIDE_BOTTOM,
	SIDE_TOP,
	SIDE_BACK,
	SIDE_FRONT,

	SIDE_COUNT
};

// Index convention used in some lookup tables
enum Edge {
	EDGE_BOTTOM_BACK = 0,
	EDGE_BOTTOM_RIGHT,
	EDGE_BOTTOM_FRONT,
	EDGE_BOTTOM_LEFT,
	EDGE_BACK_LEFT,
	EDGE_BACK_RIGHT,
	EDGE_FRONT_RIGHT,
	EDGE_FRONT_LEFT,
	EDGE_TOP_BACK,
	EDGE_TOP_RIGHT,
	EDGE_TOP_FRONT,
	EDGE_TOP_LEFT,

	EDGE_COUNT
};

// Index convention used in some lookup tables
enum Corner {
	CORNER_BOTTOM_BACK_LEFT = 0,
	CORNER_BOTTOM_BACK_RIGHT,
	CORNER_BOTTOM_FRONT_RIGHT,
	CORNER_BOTTOM_FRONT_LEFT,
	CORNER_TOP_BACK_LEFT,
	CORNER_TOP_BACK_RIGHT,
	CORNER_TOP_FRONT_RIGHT,
	CORNER_TOP_FRONT_LEFT,

	CORNER_COUNT
};

extern const Vector3 g_corner_position[CORNER_COUNT];

extern const int g_side_quad_triangles[SIDE_COUNT][6];

//extern const unsigned int g_side_coord[SIDE_COUNT];
//extern const unsigned int g_side_sign[SIDE_COUNT];

extern const Vector3i g_side_normals[SIDE_COUNT];

extern const unsigned int g_side_corners[SIDE_COUNT][4];
extern const unsigned int g_side_edges[SIDE_COUNT][4];

extern const Vector3i g_corner_inormals[CORNER_COUNT];

extern const Vector3i g_edge_inormals[EDGE_COUNT];

extern const unsigned int g_edge_corners[EDGE_COUNT][2];

const unsigned int MOORE_NEIGHBORING_3D_COUNT = 26;
extern const Vector3i g_moore_neighboring_3d[MOORE_NEIGHBORING_3D_COUNT];

} // namespace Cube

#endif // CUBE_TABLES_H
