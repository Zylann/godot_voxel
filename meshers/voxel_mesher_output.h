#ifndef VOXEL_MESHER_OUTPUT_H
#define VOXEL_MESHER_OUTPUT_H

#include "../constants/cube_tables.h"
#include "../util/containers/fixed_array.h"
#include "../util/containers/std_vector.h"
#include "../util/godot/classes/image.h"
#include "../util/godot/classes/mesh.h"
#include "../util/godot/core/array.h"
#include "../util/math/vector3f.h"
#include <cstdint>

namespace zylann::voxel {

// Due to annoying requirement that GDVIRTUAL needs the full definition of parameters, I had to separate this struct
// outside the class it was originally in.

struct VoxelMesherOutput {
	struct Surface {
		Array arrays;
		uint16_t material_index = 0;
	};
	StdVector<Surface> surfaces;
	FixedArray<StdVector<Surface>, Cube::SIDE_COUNT> transition_surfaces;
	Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;
	// Flags for creating the Godot mesh resource
	uint32_t mesh_flags = 0;

	struct CollisionSurface {
		StdVector<Vector3f> positions;
		StdVector<int> indices;
		// If >= 0, the collision surface may actually be picked from a sub-section of arrays of the first surface
		// in the render mesh (It may start from index 0).
		// Used when transition meshes are combined with the main mesh.
		int32_t submesh_vertex_end = -1;
		int32_t submesh_index_end = -1;
	};
	CollisionSurface collision_surface;

	Array shadow_occluder;

	// May be used to store extra information needed in shader to render the mesh properly
	// (currently used only by the cubes mesher when baking colors)
	Ref<Image> atlas_image;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_OUTPUT_H
