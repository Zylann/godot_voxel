#ifndef CUBE_TABLES_H
#define CUBE_TABLES_H

#include "../util/math/vector3f.h"
#include "../util/math/vector3i.h"

namespace zylann::voxel::Cube {

// Index convention used in some lookup tables
enum Side {
	SIDE_LEFT = 0,
	SIDE_RIGHT,
	SIDE_BOTTOM,
	SIDE_TOP,
	// TODO Enhancement: I would like to swap the Z ones because it's inconsistent with coordinates...
	SIDE_BACK,
	SIDE_FRONT,

	SIDE_COUNT
};
// Alias to the above for clarity, fixing some interpretation problems regarding the side_normals table...
// TODO Bug: I would like to fix the X axes, they are inverted compared to the others
enum SideAxis {
	SIDE_POSITIVE_X = 0,
	SIDE_NEGATIVE_X,
	SIDE_NEGATIVE_Y,
	SIDE_POSITIVE_Y,
	SIDE_NEGATIVE_Z,
	SIDE_POSITIVE_Z
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

extern const Vector3f g_corner_position[CORNER_COUNT];

extern const int g_side_quad_triangles[SIDE_COUNT][6];

// extern const unsigned int g_side_coord[SIDE_COUNT];
// extern const unsigned int g_side_sign[SIDE_COUNT];

extern const Vector3i g_side_normals[SIDE_COUNT];
extern const float g_side_tangents[SIDE_COUNT][4];

extern const unsigned int g_side_corners[SIDE_COUNT][4];
extern const unsigned int g_side_edges[SIDE_COUNT][4];

extern const Vector3i g_corner_inormals[CORNER_COUNT];

extern const Vector3i g_edge_inormals[EDGE_COUNT];

extern const unsigned int g_edge_corners[EDGE_COUNT][2];

const unsigned int MOORE_NEIGHBORING_3D_COUNT = 26;
extern const Vector3i g_moore_neighboring_3d[MOORE_NEIGHBORING_3D_COUNT];

const unsigned int MOORE_AREA_3D_COUNT = 27;
const unsigned int MOORE_AREA_3D_CENTRAL_INDEX = 13;
extern const Vector3i g_ordered_moore_area_3d[MOORE_AREA_3D_COUNT];

} // namespace zylann::voxel::Cube

#endif // CUBE_TABLES_H
