#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../constants/cube_tables.h"
#include "../util/fixed_array.h"

#include <scene/resources/mesh.h>
#include <vector>

namespace zylann::voxel {

namespace gd {
class VoxelBuffer;
}

class VoxelBufferInternal;
class VoxelGenerator;
struct VoxelDataLodMap;

class VoxelMesher : public Resource {
	GDCLASS(VoxelMesher, Resource)
public:
	struct Input {
		// Voxels to be used as the primary source of data.
		const VoxelBufferInternal &voxels;
		// When using LOD, some meshers can use the generator and edited voxels to affine results.
		// If not provided, the mesher will only use `voxels`.
		VoxelGenerator *generator;
		const VoxelDataLodMap *data;
		// Origin of the block is required when doing deep sampling.
		Vector3i origin_in_voxels;
		// LOD index. 0 means highest detail. 1 means half detail etc.
		// Not initialized because it confused GCC.
		int lod; // = 0;
	};

	struct Output {
		struct Surface {
			Array arrays;
		};
		// Each surface correspond to a different material and can be empty.
		std::vector<Surface> surfaces;
		FixedArray<std::vector<Surface>, Cube::SIDE_COUNT> transition_surfaces;
		Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;
		unsigned int mesh_flags = 0;
		Ref<Image> atlas_image;
	};

	// This can be called from multiple threads at once. Make sure member vars are protected or thread-local.
	virtual void build(Output &output, const Input &voxels);

	// Builds a mesh from the given voxels. This function is simplified to be used by the script API.
	Ref<Mesh> build_mesh(Ref<gd::VoxelBuffer> voxels, TypedArray<Material> materials);

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward negative axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_minimum_padding() const;

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward positive axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_maximum_padding() const;

	virtual Ref<Resource> duplicate(bool p_subresources = false) const {
		return Ref<Resource>();
	}

	// Gets which channels this mesher is able to use in its current configuration.
	// This is returned as a bitmask where channel index corresponds to bit position.
	virtual int get_used_channels_mask() const {
		return 0;
	}

	// Returns true if this mesher supports generating voxel data at multiple levels of detail.
	virtual bool supports_lod() const {
		return true;
	}

	// Some meshers can provide materials themselves. These will be used for corresponding surfaces. Returns null if the
	// index does not have a material assigned. If not provided here, a default material may be used.
	virtual Ref<Material> get_material_by_index(unsigned int i) const;

#ifdef TOOLS_ENABLED
	virtual void get_configuration_warnings(TypedArray<String> &out_warnings) const {}
#endif

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
