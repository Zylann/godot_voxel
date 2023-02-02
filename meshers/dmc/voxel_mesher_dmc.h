#ifndef VOXEL_MESHER_DMC_H
#define VOXEL_MESHER_DMC_H

#include "../../util/godot/classes/mesh.h"
#include "../../util/object_pool.h"
#include "../voxel_mesher.h"
#include "hermite_value.h"
#include "mesh_builder.h"

namespace zylann::voxel::dmc {

struct OctreeNode;
typedef zylann::ObjectPool<OctreeNode> OctreeNodePool;

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
	Vector3f corners[8];
	HermiteValue values[8];
	bool has_values = false;

	inline void set_corner(int i, Vector3f vertex, HermiteValue value) {
		CRASH_COND(i < 0 || i >= 8);
		corners[i] = vertex;
		values[i] = value;
	}
};

struct DualGrid {
	std::vector<DualCell> cells;
};

} // namespace zylann::voxel::dmc

namespace zylann::voxel {

// Mesher extending Marching Cubes using a dual grid.
class VoxelMesherDMC : public VoxelMesher {
	GDCLASS(VoxelMesherDMC, VoxelMesher)
public:
	static const int PADDING = 2;

	enum MeshMode { //
		MESH_NORMAL,
		MESH_WIREFRAME,
		MESH_DEBUG_OCTREE,
		MESH_DEBUG_DUAL_GRID
	};

	enum SimplifyMode { //
		SIMPLIFY_OCTREE_BOTTOM_UP,
		SIMPLIFY_OCTREE_TOP_DOWN,
		SIMPLIFY_NONE
	};

	enum SeamMode { //
		SEAM_NONE, // No seam management
		SEAM_MARCHING_SQUARE_SKIRTS,
		// SEAM_OVERLAP // Polygonize extra voxels with lower isolevel
		// SEAM_JUNCTION_TRIANGLES // Extra triangles for alternate borders, requires index buffer switching
	};

	VoxelMesherDMC();
	~VoxelMesherDMC();

	void set_mesh_mode(MeshMode mode);
	MeshMode get_mesh_mode() const;

	void set_simplify_mode(SimplifyMode mode);
	SimplifyMode get_simplify_mode() const;

	void set_geometric_error(real_t geometric_error);
	float get_geometric_error() const;

	void set_seam_mode(SeamMode mode);
	SeamMode get_seam_mode() const;

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	Dictionary get_statistics() const;

	Ref<Resource> duplicate(bool p_subresources = false) const ZN_OVERRIDE_UNLESS_GODOT_EXTENSION;
	int get_used_channels_mask() const override;

protected:
	static void _bind_methods();

private:
	struct Parameters {
		float geometric_error = 0.1;
		MeshMode mesh_mode = MESH_NORMAL;
		SimplifyMode simplify_mode = SIMPLIFY_OCTREE_BOTTOM_UP;
		SeamMode seam_mode = SEAM_NONE;
	};

	struct Cache {
		dmc::MeshBuilder mesh_builder;
		dmc::DualGrid dual_grid;
		dmc::OctreeNodePool octree_node_pool;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	// Work cache
	static Cache &get_tls_cache();

	struct Stats {
		float octree_build_time = 0;
		float dualgrid_derivation_time = 0;
		float meshing_time = 0;
		float commit_time = 0;
	};

	Stats _stats;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherDMC::SimplifyMode)
VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherDMC::MeshMode)
VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherDMC::SeamMode)

#endif // VOXEL_MESHER_DMC_H
