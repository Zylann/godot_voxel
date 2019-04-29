#ifndef VOXEL_MESHER_DMC_H
#define VOXEL_MESHER_DMC_H

#include "../../util/object_pool.h"
#include "../voxel_mesher.h"
#include "hermite_value.h"
#include "mesh_builder.h"
#include <scene/resources/mesh.h>

namespace dmc {

struct OctreeNode;
typedef ObjectPool<OctreeNode> OctreeNodePool;

// Octree used only for dual grid construction
struct OctreeNode {

	Vector3i origin;
	int size; // Nodes are cubic
	HermiteValue center_value;
	OctreeNode *children[8];

	OctreeNode() {
		init();
	}

	inline void init() {
		for (int i = 0; i < 8; ++i) {
			children[i] = nullptr;
		}
	}

	void recycle(OctreeNodePool &pool) {
		for (int i = 0; i < 8; ++i) {
			if (children[i]) {
				pool.recycle(children[i]);
			}
		}
	}

	inline bool has_children() const {
		return children[0] != nullptr;
	}
};

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
};

} // namespace dmc

class VoxelMesherDMC : public VoxelMesher {
	GDCLASS(VoxelMesherDMC, VoxelMesher)
public:
	static const int MINIMUM_PADDING = 2;

	enum MeshMode {
		MESH_NORMAL,
		MESH_WIREFRAME,
		MESH_DEBUG_OCTREE,
		MESH_DEBUG_DUAL_GRID
	};

	enum OctreeMode {
		OCTREE_BOTTOM_UP,
		OCTREE_TOP_DOWN,
		OCTREE_NONE
	};

	VoxelMesherDMC();

	void set_mesh_mode(MeshMode mode);
	MeshMode get_mesh_mode() const;

	void set_octree_mode(OctreeMode mode);
	OctreeMode get_octree_mode() const;

	void set_geometric_error(real_t geometric_error);
	float get_geometric_error() const;

	void build(VoxelMesher::Output &output, const VoxelBuffer &voxels, int padding) override;
	int get_minimum_padding() const override;

	Dictionary get_stats() const;

protected:
	static void _bind_methods();

private:
	dmc::MeshBuilder _mesh_builder;
	dmc::DualGrid _dual_grid;
	dmc::OctreeNodePool _octree_node_pool;
	real_t _geometric_error;
	MeshMode _mesh_mode;
	OctreeMode _octree_mode;

	struct Stats {
		real_t octree_build_time;
		real_t dualgrid_derivation_time;
		real_t meshing_time;
		real_t commit_time;
	};

	Stats _stats;
};

VARIANT_ENUM_CAST(VoxelMesherDMC::OctreeMode)
VARIANT_ENUM_CAST(VoxelMesherDMC::MeshMode)

#endif // VOXEL_MESHER_DMC_H
