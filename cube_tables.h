#ifndef CUBE_TABLES_H
#define CUBE_TABLES_H

#include <vector3.h>
#include "vector3i.h"
#include "voxel.h"

namespace CubeTables {

const unsigned int CORNER_COUNT = 8;
const unsigned int EDGE_COUNT = 12;

extern const Vector3 g_corner_position[CORNER_COUNT];

extern const int g_side_quad_triangles[Voxel::SIDE_COUNT][6];

extern const unsigned int g_side_coord[Voxel::SIDE_COUNT];
extern const unsigned int g_side_sign[Voxel::SIDE_COUNT];

extern const Vector3i g_side_normals[Voxel::SIDE_COUNT];

extern const unsigned int g_side_corners[Voxel::SIDE_COUNT][4];
extern const unsigned int g_side_edges[Voxel::SIDE_COUNT][4];

extern const Vector3i g_corner_inormals[CORNER_COUNT];

extern const Vector3i g_edge_inormals[EDGE_COUNT];

extern const unsigned int g_edge_corners[EDGE_COUNT][2];

} // namespace CubeTables


#endif // CUBE_TABLES_H
