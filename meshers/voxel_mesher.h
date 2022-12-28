#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../constants/cube_tables.h"
#include "../util/fixed_array.h"
#include "../util/godot/classes/image.h"
#include "../util/godot/classes/mesh.h"
#include "../util/macros.h"
#include "../util/span.h"
#include <vector>

ZN_GODOT_FORWARD_DECLARE(class ShaderMaterial)

namespace zylann::voxel {

namespace gd {
class VoxelBuffer;
}

class VoxelBufferInternal;
class VoxelGenerator;
class VoxelData;

// Base class for algorithms that generate meshes from voxels.
class VoxelMesher : public Resource {
	GDCLASS(VoxelMesher, Resource)
public:
	struct Input {
		// Voxels to be used as the primary source of data.
		const VoxelBufferInternal &voxels;
		// When using LOD, some meshers can use the generator and edited voxels to affine results.
		// If not provided, the mesher will only use `voxels`.
		VoxelGenerator *generator = nullptr;
		const VoxelData *data = nullptr;
		// Origin of the block is required when doing deep sampling.
		Vector3i origin_in_voxels;
		// LOD index. 0 means highest detail. 1 means half detail etc.
		uint8_t lod_index = 0;
		// If true, collision information is required.
		// Sometimes it doesn't change anything as the rendering mesh can be used as collider,
		// but in other setups it can be different and will be returned in `collision_surface`.
		bool collision_hint = false;
		// If true, the mesher is told that the mesh will be used in a context with variable level of detail.
		// For example, transition meshes will or will not be generated based on this (overriding mesher settings).
		bool lod_hint = false;
		// If true, the mesher can collect some extra information which can be useful to speed up virtual texture
		// baking. Depends on the mesher.
		bool virtual_texture_hint = false;
	};

	struct Output {
		struct Surface {
			Array arrays;
			uint8_t material_index = 0;
		};
		std::vector<Surface> surfaces;
		FixedArray<std::vector<Surface>, Cube::SIDE_COUNT> transition_surfaces;
		Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;
		// Flags for creating the Godot mesh resource
		uint32_t mesh_flags = 0;

		struct CollisionSurface {
			std::vector<Vector3f> positions;
			std::vector<int> indices;
			// If >= 0, the collision surface may actually be picked from a sub-section of arrays of the first surface
			// in the render mesh (It may start from index 0).
			// Used when transition meshes are combined with the main mesh.
			int32_t submesh_vertex_end = -1;
			int32_t submesh_index_end = -1;
		};
		CollisionSurface collision_surface;

		// May be used to store extra information needed in shader to render the mesh properly
		// (currently used only by the cubes mesher when baking colors)
		Ref<Image> atlas_image;
	};

	static bool is_mesh_empty(const std::vector<Output::Surface> &surfaces);

	// This can be called from multiple threads at once. Make sure member vars are protected or thread-local.
	virtual void build(Output &output, const Input &voxels);

	// Builds a mesh from the given voxels. This function is simplified to be used by the script API.
	Ref<Mesh> build_mesh(Ref<gd::VoxelBuffer> voxels, TypedArray<Material> materials, Dictionary additional_data);

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward negative axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_minimum_padding() const;

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward positive axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_maximum_padding() const;

	// Gets which channels this mesher is able to use in its current configuration.
	// This is returned as a bitmask where channel index corresponds to bit position.
	virtual int get_used_channels_mask() const {
		return 0;
	}

	// Returns true if this mesher supports generating voxel data at multiple levels of detail.
	virtual bool supports_lod() const {
		return true;
	}

	// Some meshers can provide materials themselves. The index may come from the built output. Returns null if the
	// index does not have a material assigned. If not provided here, a default material may be used.
	// An error can be produced if the index is out of bounds.
	virtual Ref<Material> get_material_by_index(unsigned int i) const;

#ifdef TOOLS_ENABLED
	// If the mesher has problems, messages may be returned by this method so they can be shown to the user.
	virtual void get_configuration_warnings(PackedStringArray &out_warnings) const {}
#endif

	// Returns `true` if the mesher generates specific data for mesh collisions, which will be found in
	// `CollisionSurface`.
	// If `false`, the rendering mesh may be used as collider.
	virtual bool is_generating_collision_surface() const {
		return false;
	}

	// Gets a special default material to be used to render meshes produced with this mesher, when variable level of
	// detail is used. If null, standard materials or default Godot shaders can be used. This is mostly to provide a
	// default shader that looks ok. Users are still expected to tweak them if need be.
	// Such material is not meant to be modified.
	virtual Ref<ShaderMaterial> get_default_lod_material() const;

protected:
	static void _bind_methods();

	void set_padding(int minimum, int maximum);

private:
	// Set in constructor and never changed after.
	unsigned int _minimum_padding = 0;
	unsigned int _maximum_padding = 0;
};

} // namespace zylann::voxel

#endif // VOXEL_MESHER_H
