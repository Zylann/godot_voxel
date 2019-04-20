#include "voxel_mesher_dmc.h"
#include "../cube_tables.h"
#include "../utility.h"
#include "marching_cubes_tables.h"
#include <map>
#include <vector>

// Algorithm taken from https://www.volume-gfx.com/volume-rendering/dual-marching-cubes/

namespace dmc {

enum Channels {
	CHANNEL_VALUE = 0,
	CHANNEL_GRADIENT_X,
	CHANNEL_GRADIENT_Y,
	CHANNEL_GRADIENT_Z
};

const int CHUNK_SIZE = 8;
const float ISO_LEVEL = 0.0;

struct HermiteValue {
	float value; // Signed "distance" to surface
	Vector3 gradient; // "Normal" of the volume

	HermiteValue() :
			value(1.0) {
	}
};

struct OctreeNode {
	Vector3i origin;
	int size; // Nodes are cubic
	HermiteValue center_value;
	OctreeNode *children[8];

	OctreeNode() {
		for (int i = 0; i < 8; ++i) {
			children[i] = nullptr;
		}
	}

	~OctreeNode() {
		for (int i = 0; i < 8; ++i) {
			if (children[i]) {
				memdelete(children[i]);
			}
		}
	}

	inline bool has_children() const {
		return children[0] != nullptr;
	}
};

//  Corners:                                    Octants:
//
//         6---------------18--------------7       o---o---o
//        /               /               /|       | 6 | 7 |
//       /               /               / |       o---o---o  Upper
//      17--------------25--------------19 |       | 5 | 4 |
//     /               /               /   |       o---o---o
//    /               /               /    |
//   5---------------16--------------4     |       o---o---o
//   |     14--------|-----23--------|-----15      | 2 | 3 |
//   |    /          |    /          |    /|       o---o---o  Lower        Z
//   |   /           |   /           |   / |       | 1 | 0 |               |
//   |  22-----------|--26-----------|--24 |       o---o---o           X---o
//   | /             | /             | /   |
//   |/              |/              |/    |
//   13--------------21--------------12    |
//   |     2---------|-----10--------|-----3
//   |    /          |    /          |    /
//   |   /           |   /           |   /
//   |  9------------|--20-----------|--11           Y
//   | /             | /             | /             | Z
//   |/              |/              |/              |/
//   1---------------8---------------0          X----o

const int g_octant_position[8][3]{

	{ 0, 0, 0 },
	{ 1, 0, 0 },
	{ 1, 0, 1 },
	{ 0, 0, 1 },

	{ 0, 1, 0 },
	{ 1, 1, 0 },
	{ 1, 1, 1 },
	{ 0, 1, 1 }
};

void split(OctreeNode *node) {

	CRASH_COND(node->has_children());
	CRASH_COND(node->size == 1);

	for (int i = 0; i < 8; ++i) {

		OctreeNode *child = memnew(OctreeNode);
		const int *v = g_octant_position[i];
		child->size = node->size / 2;
		child->origin = node->origin + Vector3i(v[0], v[1], v[2]) * child->size;

		node->children[i] = child;
	}
}

// Trilinear interpolation between corner values of a cube.
// Cube points respect the same position as in the ASCII schema.
template <typename T>
inline T interpolate(const T v0, const T v1, const T v2, const T v3, const T v4, const T v5, const T v6, const T v7, Vector3 position) {

	const float one_min_x = 1.f - position.x;
	const float one_min_y = 1.f - position.y;
	const float one_min_z = 1.f - position.z;
	const float one_min_x_one_min_y = one_min_x * one_min_y;
	const float x_one_min_y = position.x * one_min_y;

	T res = one_min_z * (v0 * one_min_x_one_min_y + v1 * x_one_min_y + v4 * one_min_x * position.y);
	res += position.z * (v3 * one_min_x_one_min_y + v2 * x_one_min_y + v7 * one_min_x * position.y);
	res += position.x * position.y * (v5 * one_min_z + v6 * position.z);

	return res;
}

inline Vector3 interpolate(const Vector3 &v0, const Vector3 &v1, const HermiteValue &val0, const HermiteValue &val1, Vector3 &out_normal) {

	if (Math::abs(val0.value - ISO_LEVEL) <= FLT_EPSILON) {
		out_normal = val0.gradient;
		return v0;
	}

	if (Math::abs(val1.value - ISO_LEVEL) <= FLT_EPSILON) {
		out_normal = val1.gradient;
		return v1;
	}

	if (Math::abs(val1.value - val0.value) <= FLT_EPSILON) {
		out_normal = val0.gradient;
		return v0;
	}

	float mu = (ISO_LEVEL - val0.value) / (val1.value - val0.value);
	out_normal = val0.gradient + mu * (val1.gradient - val0.gradient);
	out_normal.normalize();

	return v0 + mu * (v1 - v0);
}

inline HermiteValue get_hermite_value(const VoxelBuffer &voxels, int x, int y, int z) {
	HermiteValue v;
	v.value = voxels.get_voxel_iso(x, y, z, CHANNEL_VALUE);
	// TODO It looks like this gradient should not be a normalized vector!
	v.gradient.x = voxels.get_voxel_iso(x, y, z, CHANNEL_GRADIENT_X);
	v.gradient.y = voxels.get_voxel_iso(x, y, z, CHANNEL_GRADIENT_Y);
	v.gradient.z = voxels.get_voxel_iso(x, y, z, CHANNEL_GRADIENT_Z);
	return v;
}

inline HermiteValue get_interpolated_hermite_value(const VoxelBuffer &voxels, Vector3 pos) {

	int x0 = static_cast<int>(pos.x);
	int y0 = static_cast<int>(pos.y);
	int z0 = static_cast<int>(pos.z);

	int x1 = static_cast<int>(Math::ceil(pos.x));
	int y1 = static_cast<int>(Math::ceil(pos.y));
	int z1 = static_cast<int>(Math::ceil(pos.z));

	HermiteValue v0 = get_hermite_value(voxels, x0, y0, z0);
	HermiteValue v1 = get_hermite_value(voxels, x1, y0, z0);
	HermiteValue v2 = get_hermite_value(voxels, x1, y0, z1);
	HermiteValue v3 = get_hermite_value(voxels, x0, y0, z1);

	HermiteValue v4 = get_hermite_value(voxels, x0, y1, z0);
	HermiteValue v5 = get_hermite_value(voxels, x1, y1, z0);
	HermiteValue v6 = get_hermite_value(voxels, x1, y1, z1);
	HermiteValue v7 = get_hermite_value(voxels, x0, y1, z1);

	Vector3 rpos = pos - Vector3(x0, y0, z0);

	HermiteValue v;
	v.value = interpolate(v0.value, v1.value, v2.value, v3.value, v4.value, v5.value, v6.value, v7.value, rpos);
	v.gradient = interpolate(v0.gradient, v1.gradient, v2.gradient, v3.gradient, v4.gradient, v5.gradient, v6.gradient, v7.gradient, rpos);

	return v;
}

bool can_split(OctreeNode *node, const VoxelBuffer &voxels, float geometric_error) {

	if (node->size == 1) {
		// Voxel resolution, can't split further
		return false;
	}

	Vector3i origin = node->origin;
	int step = node->size;
	int channel = CHANNEL_VALUE;

	// Fighting with Clang-format here /**/

	float v0 = voxels.get_voxel_iso(origin.x, /*  */ origin.y, /*  */ origin.z, /*  */ channel); // 0
	float v1 = voxels.get_voxel_iso(origin.x + step, origin.y, /*  */ origin.z, /*  */ channel); // 1
	float v2 = voxels.get_voxel_iso(origin.x + step, origin.y, /*  */ origin.z + step, channel); // 2
	float v3 = voxels.get_voxel_iso(origin.x, /*  */ origin.y, /*  */ origin.z + step, channel); // 3

	float v4 = voxels.get_voxel_iso(origin.x, /*  */ origin.y + step, origin.z, /*  */ channel); // 4
	float v5 = voxels.get_voxel_iso(origin.x + step, origin.y + step, origin.z, /*  */ channel); // 5
	float v6 = voxels.get_voxel_iso(origin.x + step, origin.y + step, origin.z + step, channel); // 6
	float v7 = voxels.get_voxel_iso(origin.x, /*  */ origin.y + step, origin.z + step, channel); // 7

	int hstep = step / 2;

	Vector3i positions[19] = {
		// Starting from point 8
		Vector3i(origin.x + hstep, /**/ origin.y, /*        */ origin.z), // 8
		Vector3i(origin.x + step, /* */ origin.y, /*        */ origin.z + hstep), // 9
		Vector3i(origin.x + hstep, /**/ origin.y, /*        */ origin.z + step), // 10
		Vector3i(origin.x, /*        */ origin.y, /*        */ origin.z + hstep), // 11

		Vector3i(origin.x, /*        */ origin.y + hstep, /**/ origin.z), // 12
		Vector3i(origin.x + step, /* */ origin.y + hstep, /**/ origin.z), // 13
		Vector3i(origin.x + step, /* */ origin.y + hstep, /**/ origin.z + step), // 14
		Vector3i(origin.x, /*        */ origin.y + hstep, /**/ origin.z + step), // 15

		Vector3i(origin.x + hstep, /**/ origin.y + step, /* */ origin.z), // 16
		Vector3i(origin.x + step, /* */ origin.y + step, /* */ origin.z + hstep), // 17
		Vector3i(origin.x + hstep, /**/ origin.y + step, /* */ origin.z + step), // 18
		Vector3i(origin.x, /*        */ origin.y + step, /* */ origin.z + hstep), // 19

		Vector3i(origin.x + hstep, /**/ origin.y, /*        */ origin.z + hstep), // 20
		Vector3i(origin.x + hstep, /**/ origin.y + hstep, /**/ origin.z), // 21
		Vector3i(origin.x + step, /* */ origin.y + hstep, /**/ origin.z + hstep), // 22
		Vector3i(origin.x + hstep, /**/ origin.y + hstep, /**/ origin.z + step), // 23
		Vector3i(origin.x, /*        */ origin.y + hstep, /**/ origin.z + hstep), // 24
		Vector3i(origin.x + hstep, /**/ origin.y + step, /* */ origin.z + hstep), // 25
		Vector3i(origin.x + hstep, /**/ origin.y + hstep, /**/ origin.z + hstep) // 26
	};

	Vector3 positions_ratio[19] = {
		Vector3(0.5, 0.0, 0.0),
		Vector3(1.0, 0.0, 0.5),
		Vector3(0.5, 0.0, 1.0),
		Vector3(0.0, 0.0, 0.5),

		Vector3(0.0, 0.5, 0.0),
		Vector3(1.0, 0.5, 0.0),
		Vector3(1.0, 0.5, 1.0),
		Vector3(0.0, 0.5, 1.0),

		Vector3(0.5, 1.0, 0.0),
		Vector3(1.0, 1.0, 0.5),
		Vector3(0.5, 1.0, 1.0),
		Vector3(0.0, 1.0, 0.5),

		Vector3(0.5, 0.0, 0.5),
		Vector3(0.5, 0.5, 0.0),
		Vector3(1.0, 0.5, 0.5),
		Vector3(0.5, 0.5, 1.0),
		Vector3(0.0, 0.5, 0.5),
		Vector3(0.5, 1.0, 0.5),
		Vector3(0.5, 0.5, 0.5)
	};

	float error = 0.0;

	for (int i = 0; i < 19; ++i) {

		Vector3i pos = positions[i];

		HermiteValue value = get_hermite_value(voxels, pos.x, pos.y, pos.z);

		float interpolated_value = interpolate(v0, v1, v2, v3, v4, v5, v6, v7, positions_ratio[i]);

		float gradient_magnitude = value.gradient.length();
		if (gradient_magnitude < FLT_EPSILON) {
			gradient_magnitude = 1.0;
		}
		error += Math::abs(value.value - interpolated_value) / gradient_magnitude;
		if (error >= geometric_error) {
			return true;
		}
	}

	return false;
}

inline Vector3 get_center(const OctreeNode *node) {
	return node->origin.to_vec3() + 0.5 * Vector3(node->size, node->size, node->size);
}

// TODO There is an issue with this:
// the split policy assumes we have a real distance field, but this is not really the case.
// For example, if the volume is empty and a player places a tiny sphere in the middle,
// it might never be detected, unless the ENTIRE volume data gets affected by this tiny sphere and we use more than 8 bits per voxel.
// If that's the case, we may have to generate the octree bottom-up instead.
void generate_octree_top_down(OctreeNode *node, const VoxelBuffer &voxels, float geometric_error) {

	if (can_split(node, voxels, geometric_error)) {

		split(node);

		for (int i = 0; i < 8; ++i) {
			generate_octree_top_down(node->children[i], voxels, geometric_error);
		}

	} else {

		node->center_value = get_interpolated_hermite_value(voxels, get_center(node));
	}
}

template <typename Action_T>
void foreach_node(OctreeNode *root, Action_T &a, int depth = 0) {
	a(root, depth);
	for (int i = 0; i < 8; ++i) {
		if (root->children[i]) {
			foreach_node(root->children[i], a, depth + 1);
		}
	}
}

Ref<ArrayMesh> generate_debug_octree_mesh(OctreeNode *root) {

	struct GetMaxDepth {
		int max_depth;
		void operator()(OctreeNode *_, int depth) {
			if (depth > max_depth) {
				max_depth = depth;
			}
		}
	};

	struct Arrays {
		PoolVector3Array positions;
		PoolColorArray colors;
		PoolIntArray indices;
	};

	struct AddCube {
		Arrays *arrays;
		int max_depth;

		void operator()(OctreeNode *node, int depth) {

			float shrink = depth * 0.005;
			Vector3 o = node->origin.to_vec3() + Vector3(shrink, shrink, shrink);
			float s = node->size - 2.0 * shrink;

			Color col(1.0, (float)depth / (float)max_depth, 0.0);

			int vi = arrays->positions.size();

			for (int i = 0; i < Cube::CORNER_COUNT; ++i) {
				arrays->positions.push_back(o + s * Cube::g_corner_position[i]);
				arrays->colors.push_back(col);
			}

			for (int i = 0; i < Cube::EDGE_COUNT; ++i) {
				arrays->indices.push_back(vi + Cube::g_edge_corners[i][0]);
				arrays->indices.push_back(vi + Cube::g_edge_corners[i][1]);
			}
		}
	};

	GetMaxDepth get_max_depth;
	foreach_node(root, get_max_depth);

	Arrays arrays;
	AddCube add_cube;
	add_cube.arrays = &arrays;
	add_cube.max_depth = get_max_depth.max_depth;
	foreach_node(root, add_cube);

	if (arrays.positions.size() == 0) {
		return Ref<ArrayMesh>();
	}

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);
	surface[Mesh::ARRAY_VERTEX] = arrays.positions;
	surface[Mesh::ARRAY_COLOR] = arrays.colors;
	surface[Mesh::ARRAY_INDEX] = arrays.indices;

	Ref<ArrayMesh> mesh;
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, surface);

	return mesh;
}

inline bool is_surface_near(OctreeNode *node) {

	if (node->center_value.value == 0) {
		return true;
	}

	const float sqrt3 = 1.7320508075688772;
	const float near_factor = 2.f;

	return Math::abs(node->center_value.value) < node->size * sqrt3 * near_factor;
}

struct DualCell {
	Vector3 corners[8];
	HermiteValue values[8];
	bool has_values;

	DualCell() :
			has_values(false) {}

	inline void set_corner(int i, Vector3 vertex, HermiteValue value) {
		CRASH_COND(i < 0 || i >= 8);
		corners[i] = vertex;
		values[i] = value;
	}
};

struct DualGrid {
	std::vector<DualCell> cells;

	void add_cell(const Vector3 c0, const Vector3 c1, const Vector3 c2, const Vector3 c3, const Vector3 c4, const Vector3 c5, const Vector3 c6, const Vector3 c7) {
		DualCell cell;
		cell.corners[0] = c0;
		cell.corners[1] = c1;
		cell.corners[2] = c2;
		cell.corners[3] = c3;
		cell.corners[4] = c4;
		cell.corners[5] = c5;
		cell.corners[6] = c6;
		cell.corners[7] = c7;
		cells.push_back(cell);
	}
};

Ref<ArrayMesh> generate_debug_dual_grid_mesh(const DualGrid &grid) {

	PoolVector3Array positions;
	PoolIntArray indices;

	for (int i = 0; i < grid.cells.size(); ++i) {

		const DualCell &cell = grid.cells[i];

		int vi = positions.size();

		for (int j = 0; j < 8; ++j) {
			positions.push_back(cell.corners[j]);
		}

		for (int j = 0; j < Cube::EDGE_COUNT; ++j) {
			indices.push_back(vi + Cube::g_edge_corners[j][0]);
			indices.push_back(vi + Cube::g_edge_corners[j][1]);
		}
	}

	if (positions.size() == 0) {
		return Ref<ArrayMesh>();
	}

	Array surface;
	surface.resize(Mesh::ARRAY_MAX);
	surface[Mesh::ARRAY_VERTEX] = positions;
	surface[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instance();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, surface);

	return mesh;
}

inline bool is_border_left(const OctreeNode *node) {
	return node->origin.x == 0;
}

inline bool is_border_right(const OctreeNode *node) {
	return node->origin.x + node->size == CHUNK_SIZE;
}

inline bool is_border_bottom(const OctreeNode *node) {
	return node->origin.y == 0;
}

inline bool is_border_top(const OctreeNode *node) {
	return node->origin.y + node->size == CHUNK_SIZE;
}

inline bool is_border_back(const OctreeNode *node) {
	return node->origin.z == 0;
}

inline bool is_border_front(const OctreeNode *node) {
	return node->origin.z + node->size == CHUNK_SIZE;
}

inline Vector3 get_center_back(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.y += node->size * 0.5;
	return p;
}

inline Vector3 get_center_front(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.y += node->size * 0.5;
	p.z += node->size;
	return p;
}

inline Vector3 get_center_left(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size * 0.5;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_right(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size * 0.5;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_top(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.y += node->size;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_bottom(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_back_top(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.y += node->size;
	return p;
}

inline Vector3 get_center_back_bottom(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	return p;
}

inline Vector3 get_center_front_top(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.y += node->size;
	p.z += node->size;
	return p;
}

inline Vector3 get_center_front_bottom(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size * 0.5;
	p.z += node->size;
	return p;
}

inline Vector3 get_center_left_top(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_left_bottom(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_right_top(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_right_bottom(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.z += node->size * 0.5;
	return p;
}

inline Vector3 get_center_back_left(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size * 0.5;
	return p;
}

inline Vector3 get_center_front_left(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size * 0.5;
	p.z += node->size;
	return p;
}

inline Vector3 get_center_back_right(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size * 0.5;
	return p;
}

inline Vector3 get_center_front_right(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size * 0.5;
	p.z += node->size;
	return p;
}

inline Vector3 get_corner1(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	return p;
}

inline Vector3 get_corner2(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.z += node->size;
	return p;
}

inline Vector3 get_corner3(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.z += node->size;
	return p;
}

inline Vector3 get_corner4(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size;
	return p;
}

inline Vector3 get_corner5(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size;
	return p;
}

inline Vector3 get_corner6(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.x += node->size;
	p.y += node->size;
	p.z += node->size;
	return p;
}

inline Vector3 get_corner7(const OctreeNode *node) {
	Vector3 p = node->origin.to_vec3();
	p.y += node->size;
	p.z += node->size;
	return p;
}

void create_border_cells(DualGrid &grid,
		const OctreeNode *n0,
		const OctreeNode *n1,
		const OctreeNode *n2,
		const OctreeNode *n3,
		const OctreeNode *n4,
		const OctreeNode *n5,
		const OctreeNode *n6,
		const OctreeNode *n7) {

	// Most boring function ever

	if (is_border_back(n0) && is_border_back(n1) && is_border_back(n4) && is_border_back(n5)) {

		grid.add_cell(
				get_center_back(n0), get_center_back(n1), get_center(n1), get_center(n0),
				get_center_back(n4), get_center_back(n5), get_center(n5), get_center(n4));

		// Generate back edge border cells
		if (is_border_top(n4) && is_border_top(n5)) {

			grid.add_cell(
					get_center_back(n4), get_center_back(n5), get_center(n5), get_center(n4),
					get_center_back_top(n4), get_center_back_top(n5), get_center_top(n5), get_center_top(n4));

			// Generate back top corner cells
			if (is_border_left(n4)) {
				grid.add_cell(
						get_center_back_left(n4), get_center_back(n4), get_center(n4), get_center_left(n4),
						get_corner4(n4), get_center_back_top(n4), get_center_top(n4), get_center_left_top(n4));
			}

			if (is_border_right(n4)) {
				grid.add_cell(
						get_center_back(n5), get_center_back_right(n5), get_center_right(n5), get_center(n5),
						get_center_back_top(n5), get_corner5(n5), get_center_right_top(n5), get_center_top(n5));
			}
		}

		if (is_border_bottom(n0) && is_border_bottom(n1)) {

			grid.add_cell(
					get_center_back_bottom(n0), get_center_back_bottom(n1), get_center_bottom(n1), get_center_bottom(n0),
					get_center_back(n0), get_center_back(n1), get_center(n1), get_center(n0));

			// Generate back bottom corner cells
			if (is_border_left(n0)) {
				grid.add_cell(n0->origin.to_vec3(), get_center_back_bottom(n0), get_center_bottom(n0), get_center_left_bottom(n0),
						get_center_back_left(n0), get_center_back(n0), get_center(n0), get_center_left(n0));
			}

			if (is_border_right(n1)) {
				grid.add_cell(get_center_back_bottom(n1), get_corner1(n1), get_center_right_bottom(n1), get_center_bottom(n1),
						get_center_back(n1), get_center_back_right(n1), get_center_right(n1), get_center(n1));
			}
		}
	}

	if (is_border_front(n2) && is_border_front(n3) && is_border_front(n6) && is_border_front(n7)) {

		grid.add_cell(
				get_center(n3), get_center(n2), get_center_front(n2), get_center_front(n3),
				get_center(n7), get_center(n6), get_center_front(n6), get_center_front(n7));

		// Generate front edge border cells
		if (is_border_top(n6) && is_border_top(n7)) {

			grid.add_cell(
					get_center(n7), get_center(n6), get_center_front(n6), get_center_front(n7),
					get_center_top(n7), get_center_top(n6), get_center_front_top(n6), get_center_front_top(n7));

			// Generate back bottom corner cells
			if (is_border_left(n7)) {
				grid.add_cell(
						get_center_left(n7), get_center(n7), get_center_front(n7), get_center_front_left(n7),
						get_center_left_top(n7), get_center_top(n7), get_center_front_top(n7), get_corner7(n7));
			}

			if (is_border_right(n6)) {
				grid.add_cell(
						get_center(n6), get_center_right(n6), get_center_front_right(n6), get_center_front(n6),
						get_center_top(n6), get_center_right_top(n6), get_corner6(n6), get_center_front_top(n6));
			}
		}

		if (is_border_bottom(n3) && is_border_bottom(n2)) {

			grid.add_cell(
					get_center_bottom(n3), get_center_bottom(n2), get_center_front_bottom(n2), get_center_front_bottom(n3),
					get_center(n3), get_center(n2), get_center_front(n2), get_center_front(n3));

			// Generate back bottom corner cells
			if (is_border_left(n3)) {
				grid.add_cell(
						get_center_left_bottom(n3), get_center_bottom(n3), get_center_front_bottom(n3), get_corner3(n3),
						get_center_left(n3), get_center(n3), get_center_front(n3), get_center_front_left(n3));
			}
			if (is_border_right(n2)) {
				grid.add_cell(get_center_bottom(n2), get_center_right_bottom(n2), get_corner2(n2), get_center_front_bottom(n2),
						get_center(n2), get_center_right(n2), get_center_front_right(n2), get_center_front(n2));
			}
		}
	}

	if (is_border_left(n0) && is_border_left(n3) && is_border_left(n4) && is_border_left(n7)) {

		grid.add_cell(
				get_center_left(n0), get_center(n0), get_center(n3), get_center_left(n3),
				get_center_left(n4), get_center(n4), get_center(n7), get_center_left(n7));

		// Generate left edge border cells
		if (is_border_top(n4) && is_border_top(n7)) {
			grid.add_cell(
					get_center_left(n4), get_center(n4), get_center(n7), get_center_left(n7),
					get_center_left_top(n4), get_center_top(n4), get_center_top(n7), get_center_left_top(n7));
		}

		if (is_border_bottom(n0) && is_border_bottom(n3)) {
			grid.add_cell(
					get_center_left_bottom(n0), get_center_bottom(n0), get_center_bottom(n3), get_center_left_bottom(n3),
					get_center_left(n0), get_center(n0), get_center(n3), get_center_left(n3));
		}

		if (is_border_back(n0) && is_border_back(n4)) {
			grid.add_cell(
					get_center_back_left(n0), get_center_back(n0), get_center(n0), get_center_left(n0),
					get_center_back_left(n4), get_center_back(n4), get_center(n4), get_center_left(n4));
		}

		if (is_border_front(n3) && is_border_front(n7)) {
			grid.add_cell(
					get_center_left(n3), get_center(n3), get_center_front(n3), get_center_front_left(n3),
					get_center_left(n7), get_center(n7), get_center_front(n7), get_center_front_left(n7));
		}
	}

	if (is_border_right(n1) && is_border_right(n2) && is_border_right(n5) && is_border_right(n6)) {

		grid.add_cell(
				get_center(n1), get_center_right(n1), get_center_right(n2), get_center(n2),
				get_center(n5), get_center_right(n5), get_center_right(n6), get_center(n6));

		// Generate right edge border cells
		if (is_border_top(n5) && is_border_top(n6)) {
			grid.add_cell(
					get_center(n5), get_center_right(n5), get_center_right(n6), get_center(n6),
					get_center_top(n5), get_center_right_top(n5), get_center_right_top(n6), get_center_top(n6));
		}

		if (is_border_bottom(n1) && is_border_bottom(n2)) {
			grid.add_cell(
					get_center_bottom(n1), get_center_right_bottom(n1), get_center_right_bottom(n2), get_center_bottom(n2),
					get_center(n1), get_center_right(n1), get_center_right(n2), get_center(n2));
		}

		if (is_border_back(n1) && is_border_back(n5)) {
			grid.add_cell(
					get_center_back(n1), get_center_back_right(n1), get_center_right(n1), get_center(n1),
					get_center_back(n5), get_center_back_right(n5), get_center_right(n5), get_center(n5));
		}

		if (is_border_front(n2) && is_border_front(n6)) {
			grid.add_cell(
					get_center(n2), get_center_right(n2), get_center_front_right(n2), get_center_front(n2),
					get_center(n6), get_center_right(n6), get_center_front_right(n6), get_center_front(n6));
		}
	}

	if (is_border_top(n4) && is_border_top(n5) && is_border_top(n6) && is_border_top(n7)) {
		grid.add_cell(
				get_center(n4), get_center(n5), get_center(n6), get_center(n7),
				get_center_top(n4), get_center_top(n5), get_center_top(n6), get_center_top(n7));
	}

	if (is_border_bottom(n0) && is_border_bottom(n1) && is_border_bottom(n2) && is_border_bottom(n3)) {
		grid.add_cell(
				get_center_bottom(n0), get_center_bottom(n1), get_center_bottom(n2), get_center_bottom(n3),
				get_center(n0), get_center(n1), get_center(n2), get_center(n3));
	}
}

void vert_proc(DualGrid &grid, OctreeNode *n0, OctreeNode *n1, OctreeNode *n2, OctreeNode *n3, OctreeNode *n4, OctreeNode *n5, OctreeNode *n6, OctreeNode *n7) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();
	const bool n2_has_children = n2->has_children();
	const bool n3_has_children = n3->has_children();
	const bool n4_has_children = n4->has_children();
	const bool n5_has_children = n5->has_children();
	const bool n6_has_children = n6->has_children();
	const bool n7_has_children = n7->has_children();

	if (n0_has_children || n1_has_children || n2_has_children || n3_has_children || n4_has_children || n5_has_children || n6_has_children || n7_has_children) {

		OctreeNode *c0 = n0_has_children ? n0->children[6] : n0;
		OctreeNode *c1 = n1_has_children ? n1->children[7] : n1;
		OctreeNode *c2 = n2_has_children ? n2->children[4] : n2;
		OctreeNode *c3 = n3_has_children ? n3->children[5] : n3;
		OctreeNode *c4 = n4_has_children ? n4->children[2] : n4;
		OctreeNode *c5 = n5_has_children ? n5->children[3] : n5;
		OctreeNode *c6 = n6_has_children ? n6->children[0] : n6;
		OctreeNode *c7 = n7_has_children ? n7->children[1] : n7;

		vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);

	} else {

		if (!(is_surface_near(n0) ||
					is_surface_near(n1) ||
					is_surface_near(n2) ||
					is_surface_near(n3) ||
					is_surface_near(n4) ||
					is_surface_near(n5) ||
					is_surface_near(n6) ||
					is_surface_near(n7))) {
			return;
		}

		DualCell cell;
		cell.set_corner(0, get_center(n0), n0->center_value);
		cell.set_corner(1, get_center(n1), n1->center_value);
		cell.set_corner(2, get_center(n2), n2->center_value);
		cell.set_corner(3, get_center(n3), n3->center_value);
		cell.set_corner(4, get_center(n4), n4->center_value);
		cell.set_corner(5, get_center(n5), n5->center_value);
		cell.set_corner(6, get_center(n6), n6->center_value);
		cell.set_corner(7, get_center(n7), n7->center_value);
		cell.has_values = true;
		grid.cells.push_back(cell);

		create_border_cells(grid, n0, n1, n2, n3, n4, n5, n6, n7);
	}
}

void edge_proc_x(DualGrid &grid, OctreeNode *n0, OctreeNode *n1, OctreeNode *n2, OctreeNode *n3) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();
	const bool n2_has_children = n2->has_children();
	const bool n3_has_children = n3->has_children();

	if (!(n0_has_children || n0_has_children || n2_has_children || n3_has_children)) {
		return;
	}

	OctreeNode *c0 = n0_has_children ? n0->children[7] : n0;
	OctreeNode *c1 = n0_has_children ? n0->children[6] : n0;
	OctreeNode *c2 = n1_has_children ? n1->children[5] : n1;
	OctreeNode *c3 = n1_has_children ? n1->children[4] : n1;
	OctreeNode *c4 = n3_has_children ? n3->children[3] : n3;
	OctreeNode *c5 = n3_has_children ? n3->children[2] : n3;
	OctreeNode *c6 = n2_has_children ? n2->children[1] : n2;
	OctreeNode *c7 = n2_has_children ? n2->children[0] : n2;

	edge_proc_x(grid, c0, c3, c7, c4);
	edge_proc_x(grid, c1, c2, c6, c5);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void edge_proc_y(DualGrid &grid, OctreeNode *n0, OctreeNode *n1, OctreeNode *n2, OctreeNode *n3) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();
	const bool n2_has_children = n2->has_children();
	const bool n3_has_children = n3->has_children();

	if (!(n0_has_children || n1_has_children || n2_has_children || n3_has_children)) {
		return;
	}

	OctreeNode *c0 = n0_has_children ? n0->children[2] : n0;
	OctreeNode *c1 = n1_has_children ? n1->children[3] : n1;
	OctreeNode *c2 = n2_has_children ? n2->children[0] : n2;
	OctreeNode *c3 = n3_has_children ? n3->children[1] : n3;
	OctreeNode *c4 = n0_has_children ? n0->children[6] : n0;
	OctreeNode *c5 = n1_has_children ? n1->children[7] : n1;
	OctreeNode *c6 = n2_has_children ? n2->children[4] : n2;
	OctreeNode *c7 = n3_has_children ? n3->children[5] : n3;

	edge_proc_y(grid, c0, c1, c2, c3);
	edge_proc_y(grid, c4, c5, c6, c7);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void edge_proc_z(DualGrid &grid, OctreeNode *n0, OctreeNode *n1, OctreeNode *n2, OctreeNode *n3) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();
	const bool n2_has_children = n2->has_children();
	const bool n3_has_children = n3->has_children();

	if (!(n0_has_children || n1_has_children || n2_has_children || n3_has_children)) {
		return;
	}

	OctreeNode *c0 = n3_has_children ? n3->children[5] : n3;
	OctreeNode *c1 = n2_has_children ? n2->children[4] : n2;
	OctreeNode *c2 = n2_has_children ? n2->children[7] : n2;
	OctreeNode *c3 = n3_has_children ? n3->children[6] : n3;
	OctreeNode *c4 = n0_has_children ? n0->children[1] : n0;
	OctreeNode *c5 = n1_has_children ? n1->children[0] : n1;
	OctreeNode *c6 = n1_has_children ? n1->children[3] : n1;
	OctreeNode *c7 = n0_has_children ? n0->children[2] : n0;

	edge_proc_y(grid, c0, c1, c2, c3);
	edge_proc_y(grid, c4, c5, c6, c7);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void face_proc_xy(DualGrid &grid, OctreeNode *n0, OctreeNode *n1) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();

	if (!(n0_has_children || n1_has_children)) {
		return;
	}

	OctreeNode *c0 = n0_has_children ? n0->children[3] : n0;
	OctreeNode *c1 = n0_has_children ? n0->children[2] : n0;
	OctreeNode *c2 = n1_has_children ? n1->children[1] : n1;
	OctreeNode *c3 = n1_has_children ? n1->children[0] : n1;
	OctreeNode *c4 = n0_has_children ? n0->children[7] : n0;
	OctreeNode *c5 = n0_has_children ? n0->children[6] : n0;
	OctreeNode *c6 = n1_has_children ? n1->children[5] : n1;
	OctreeNode *c7 = n1_has_children ? n1->children[4] : n1;

	face_proc_xy(grid, c0, c3);
	face_proc_xy(grid, c1, c2);
	face_proc_xy(grid, c4, c7);
	face_proc_xy(grid, c5, c6);

	edge_proc_x(grid, c0, c3, c7, c4);
	edge_proc_x(grid, c1, c2, c6, c5);

	edge_proc_y(grid, c0, c1, c2, c3);
	edge_proc_y(grid, c4, c5, c6, c7);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void face_proc_zy(DualGrid &grid, OctreeNode *n0, OctreeNode *n1) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();

	if (!(n0_has_children || n1_has_children)) {
		return;
	}

	OctreeNode *c0 = n0_has_children ? n0->children[1] : n0;
	OctreeNode *c1 = n1_has_children ? n1->children[0] : n1;
	OctreeNode *c2 = n1_has_children ? n1->children[3] : n1;
	OctreeNode *c3 = n0_has_children ? n0->children[2] : n0;
	OctreeNode *c4 = n0_has_children ? n0->children[5] : n0;
	OctreeNode *c5 = n1_has_children ? n1->children[4] : n1;
	OctreeNode *c6 = n1_has_children ? n1->children[7] : n1;
	OctreeNode *c7 = n0_has_children ? n0->children[6] : n0;

	face_proc_zy(grid, c0, c1);
	face_proc_zy(grid, c3, c2);
	face_proc_zy(grid, c4, c5);
	face_proc_zy(grid, c7, c6);

	edge_proc_y(grid, c0, c1, c2, c3);
	edge_proc_y(grid, c4, c5, c6, c7);
	edge_proc_z(grid, c7, c6, c2, c3);
	edge_proc_z(grid, c4, c5, c1, c0);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void face_proc_xz(DualGrid &grid, OctreeNode *n0, OctreeNode *n1) {

	const bool n0_has_children = n0->has_children();
	const bool n1_has_children = n1->has_children();

	if (!(n0_has_children || n1_has_children)) {
		return;
	}

	OctreeNode *c0 = n1_has_children ? n1->children[4] : n1;
	OctreeNode *c1 = n1_has_children ? n1->children[5] : n1;
	OctreeNode *c2 = n1_has_children ? n1->children[6] : n1;
	OctreeNode *c3 = n1_has_children ? n1->children[7] : n1;
	OctreeNode *c4 = n0_has_children ? n0->children[0] : n0;
	OctreeNode *c5 = n0_has_children ? n0->children[1] : n0;
	OctreeNode *c6 = n0_has_children ? n0->children[2] : n0;
	OctreeNode *c7 = n0_has_children ? n0->children[3] : n0;

	face_proc_xz(grid, c4, c0);
	face_proc_xz(grid, c5, c1);
	face_proc_xz(grid, c7, c3);
	face_proc_xz(grid, c6, c2);

	edge_proc_x(grid, c0, c3, c7, c4);
	edge_proc_x(grid, c1, c2, c6, c5);
	edge_proc_z(grid, c7, c6, c2, c3);
	edge_proc_z(grid, c4, c5, c1, c0);

	vert_proc(grid, c0, c1, c2, c3, c4, c5, c6, c7);
}

void node_proc(DualGrid &grid, OctreeNode *node) {

	if (!node->has_children()) {
		return;
	}

	OctreeNode **children = node->children;

	for (int i = 0; i < 8; ++i) {
		node_proc(grid, children[i]);
	}

	face_proc_xy(grid, children[0], children[3]);
	face_proc_xy(grid, children[1], children[2]);
	face_proc_xy(grid, children[4], children[7]);
	face_proc_xy(grid, children[5], children[6]);

	face_proc_zy(grid, children[0], children[1]);
	face_proc_zy(grid, children[3], children[2]);
	face_proc_zy(grid, children[4], children[5]);
	face_proc_zy(grid, children[7], children[6]);

	face_proc_xz(grid, children[4], children[0]);
	face_proc_xz(grid, children[5], children[1]);
	face_proc_xz(grid, children[7], children[3]);
	face_proc_xz(grid, children[6], children[2]);

	edge_proc_x(grid, children[0], children[3], children[7], children[4]);
	edge_proc_x(grid, children[1], children[2], children[6], children[5]);

	edge_proc_y(grid, children[0], children[1], children[2], children[3]);
	edge_proc_y(grid, children[4], children[5], children[6], children[7]);

	edge_proc_z(grid, children[7], children[6], children[2], children[3]);
	edge_proc_z(grid, children[4], children[5], children[1], children[0]);

	vert_proc(grid, children[0], children[1], children[2], children[3], children[4], children[5], children[6], children[7]);
}

class MeshBuilder {
public:
	void add_vertex(Vector3 position, Vector3 normal) {

		int i = 0;

		if (_position_to_index.find(position) != _position_to_index.end()) {

			i = _position_to_index[position];

		} else {

			i = _positions.size();
			_position_to_index[position] = i;

			_positions.push_back(position);
			_normals.push_back(normal);
		}

		_indices.push_back(i);
	}

	Ref<ArrayMesh> commit(bool wireframe) {

		if (_positions.size() == 0) {
			return Ref<ArrayMesh>();
		}

		ERR_FAIL_COND_V(_indices.size() % 3 != 0, Ref<ArrayMesh>());

		if (wireframe) {

			// Debug purpose, no effort to be fast here
			std::vector<int> wireframe_indices;

			for (int i = 0; i < _indices.size(); i += 3) {

				wireframe_indices.push_back(_indices[i]);
				wireframe_indices.push_back(_indices[i + 1]);

				wireframe_indices.push_back(_indices[i + 1]);
				wireframe_indices.push_back(_indices[i + 2]);

				wireframe_indices.push_back(_indices[i + 2]);
				wireframe_indices.push_back(_indices[i]);
			}

			_indices = wireframe_indices;
		}

		PoolVector3Array positions;
		PoolVector3Array normals;
		PoolIntArray indices;

		raw_copy_to(positions, _positions);
		raw_copy_to(normals, _normals);
		raw_copy_to(indices, _indices);

		Array surface;
		surface.resize(Mesh::ARRAY_MAX);
		surface[Mesh::ARRAY_VERTEX] = positions;
		surface[Mesh::ARRAY_NORMAL] = normals;
		surface[Mesh::ARRAY_INDEX] = indices;

		Ref<ArrayMesh> mesh;
		mesh.instance();
		mesh->add_surface_from_arrays(wireframe ? Mesh::PRIMITIVE_LINES : Mesh::PRIMITIVE_TRIANGLES, surface);

		return mesh;
	}

private:
	std::vector<Vector3> _positions;
	std::vector<Vector3> _normals;
	std::vector<int> _indices;
	std::map<Vector3, int> _position_to_index;
};

Ref<ArrayMesh> polygonize_dual_grid(const DualGrid &grid, const VoxelBuffer &voxels, bool wireframe) {

	MeshBuilder mesh_builder;

	for (int dci = 0; dci < grid.cells.size(); ++dci) {

		const DualCell &cell = grid.cells[dci];
		const Vector3 *corners = cell.corners;

		// Polygonize using regular marching cubes
		unsigned char case_index = 0;
		HermiteValue values[8];

		for (int i = 0; i < 8; ++i) {
			if (cell.has_values) {
				values[i] = cell.values[i];
			} else {
				values[i] = get_interpolated_hermite_value(voxels, corners[i]);
			}
			if (values[i].value >= ISO_LEVEL) {
				case_index |= 1 << i;
			}
		}

		int edge = MarchingCubes::mc_edges[case_index];

		if (!edge) {
			// Nothing intersects
			continue;
		}

		// Find the intersection vertices
		Vector3 intersection_points[12];
		Vector3 intersection_normals[12];
		if (edge & 1) {
			intersection_points[0] = interpolate(corners[0], corners[1], values[0], values[1], intersection_normals[0]);
		}
		if (edge & 2) {
			intersection_points[1] = interpolate(corners[1], corners[2], values[1], values[2], intersection_normals[1]);
		}
		if (edge & 4) {
			intersection_points[2] = interpolate(corners[2], corners[3], values[2], values[3], intersection_normals[2]);
		}
		if (edge & 8) {
			intersection_points[3] = interpolate(corners[3], corners[0], values[3], values[0], intersection_normals[3]);
		}
		if (edge & 16) {
			intersection_points[4] = interpolate(corners[4], corners[5], values[4], values[5], intersection_normals[4]);
		}
		if (edge & 32) {
			intersection_points[5] = interpolate(corners[5], corners[6], values[5], values[6], intersection_normals[5]);
		}
		if (edge & 64) {
			intersection_points[6] = interpolate(corners[6], corners[7], values[6], values[7], intersection_normals[6]);
		}
		if (edge & 128) {
			intersection_points[7] = interpolate(corners[7], corners[4], values[7], values[4], intersection_normals[7]);
		}
		if (edge & 256) {
			intersection_points[8] = interpolate(corners[0], corners[4], values[0], values[4], intersection_normals[8]);
		}
		if (edge & 512) {
			intersection_points[9] = interpolate(corners[1], corners[5], values[1], values[5], intersection_normals[9]);
		}
		if (edge & 1024) {
			intersection_points[10] = interpolate(corners[2], corners[6], values[2], values[6], intersection_normals[10]);
		}
		if (edge & 2048) {
			intersection_points[11] = interpolate(corners[3], corners[7], values[3], values[7], intersection_normals[11]);
		}

		// Create the triangles according to the table.
		for (int i = 0; MarchingCubes::mc_triangles[case_index][i] != -1; i += 3) {

			mesh_builder.add_vertex(
					intersection_points[MarchingCubes::mc_triangles[case_index][i]],
					intersection_normals[MarchingCubes::mc_triangles[case_index][i]]);

			mesh_builder.add_vertex(
					intersection_points[MarchingCubes::mc_triangles[case_index][i + 1]],
					intersection_normals[MarchingCubes::mc_triangles[case_index][i + 1]]);

			mesh_builder.add_vertex(
					intersection_points[MarchingCubes::mc_triangles[case_index][i + 2]],
					intersection_normals[MarchingCubes::mc_triangles[case_index][i + 2]]);
		}
	}

	return mesh_builder.commit(wireframe);
}

Ref<ArrayMesh> polygonize(const VoxelBuffer &voxels, float geometric_error, VoxelMesherDMC::Mode mode) {

	int padding = 1;
	int chunk_size = CHUNK_SIZE;
	// TODO Don't hardcode size. It must be next lower power of two

	CRASH_COND(voxels.get_size().x < chunk_size + padding * 2);
	CRASH_COND(voxels.get_size().y < chunk_size + padding * 2);
	CRASH_COND(voxels.get_size().z < chunk_size + padding * 2);

	OctreeNode root;
	root.origin = Vector3i();
	root.size = chunk_size;
	generate_octree_top_down(&root, voxels, geometric_error);

	if (mode == VoxelMesherDMC::MODE_DEBUG_OCTREE) {
		return generate_debug_octree_mesh(&root);
	}

	DualGrid grid;
	node_proc(grid, &root);
	// TODO Handle non-subdivided octree
	if (mode == VoxelMesherDMC::MODE_DEBUG_DUAL_GRID) {
		return generate_debug_dual_grid_mesh(grid);
	}

	return polygonize_dual_grid(grid, voxels, mode == VoxelMesherDMC::MODE_WIREFRAME);
	// TODO Marching squares skirts
}

} // namespace dmc

Ref<ArrayMesh> VoxelMesherDMC::build_mesh(Ref<VoxelBuffer> voxels, real_t geometric_error, Mode mode) {
	ERR_FAIL_COND_V(voxels.is_null(), Ref<ArrayMesh>());
	return dmc::polygonize(**voxels, geometric_error, mode);
}

void VoxelMesherDMC::_bind_methods() {

	ClassDB::bind_method(D_METHOD("build_mesh", "voxel_buffer", "geometric_error", "mode"), &VoxelMesherDMC::build_mesh, DEFVAL(MODE_NORMAL));

	BIND_ENUM_CONSTANT(MODE_NORMAL);
	BIND_ENUM_CONSTANT(MODE_WIREFRAME);
	BIND_ENUM_CONSTANT(MODE_DEBUG_OCTREE);
	BIND_ENUM_CONSTANT(MODE_DEBUG_DUAL_GRID);
}
