#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../constants/cube_tables.h"
#include "../util/fixed_array.h"
#include <scene/resources/mesh.h>

class VoxelBuffer;
class VoxelBufferInternal;

class VoxelMesher : public Resource {
	GDCLASS(VoxelMesher, Resource)
public:
	struct Input {
		const VoxelBufferInternal &voxels;
		int lod; // = 0; // Not initialized because it confused GCC
	};

	struct Output {
		// Each surface correspond to a different material
		Vector<Array> surfaces;
		FixedArray<Vector<Array>, Cube::SIDE_COUNT> transition_surfaces;
		Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;
		unsigned int compression_flags = Mesh::ARRAY_COMPRESS_DEFAULT;
		Ref<Image> atlas_image;
	};

	// This can be called from multiple threads at once. Make sure member vars are protected or thread-local.
	virtual void build(Output &output, const Input &voxels);

	// Builds a mesh from the given voxels. This function is simplified to be used by the script API.
	Ref<Mesh> build_mesh(Ref<VoxelBuffer> voxels, Array materials);

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward negative axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_minimum_padding() const;

	// Gets how many neighbor voxels need to be accessed around the meshed area, toward positive axes.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	unsigned int get_maximum_padding() const;

	virtual Ref<Resource> duplicate(bool p_subresources = false) const { return Ref<Resource>(); }

	// Gets which channels this mesher is able to use in its current configuration.
	// This is returned as a bitmask where channel index corresponds to bit position.
	virtual int get_used_channels_mask() const { return 0; }

	// Returns true if this mesher supports generating voxel data at multiple levels of detail.
	virtual bool supports_lod() const { return true; }

protected:
	static void _bind_methods();

	void set_padding(int minimum, int maximum);

private:
	// Set in constructor and never changed after.
	unsigned int _minimum_padding = 0;
	unsigned int _maximum_padding = 0;
};

#endif // VOXEL_MESHER_H
