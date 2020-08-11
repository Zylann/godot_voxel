#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../cube_tables.h"
#include "../util/fixed_array.h"
#include "../voxel_buffer.h"
#include <scene/resources/mesh.h>

class VoxelMesher : public Reference {
	GDCLASS(VoxelMesher, Reference)
public:
	struct Input {
		const VoxelBuffer &voxels;
		int lod; // = 0; // Not initialized because it confused GCC
	};

	struct Output {
		// Each surface correspond to a different material
		Vector<Array> surfaces;
		FixedArray<Vector<Array>, Cube::SIDE_COUNT> transition_surfaces;
		Mesh::PrimitiveType primitive_type = Mesh::PRIMITIVE_TRIANGLES;
		unsigned int compression_flags = Mesh::ARRAY_COMPRESS_DEFAULT;
	};

	virtual void build(Output &output, const Input &voxels);

	// Get how many neighbor voxels need to be accessed around the meshed area.
	// If this is not respected, the mesher might produce seams at the edges, or an error
	int get_minimum_padding() const;
	int get_maximum_padding() const;

	// TODO Rename duplicate()
	// Must be cloneable so can be duplicated for use by more than one thread
	virtual VoxelMesher *clone();

	Ref<Mesh> build_mesh(Ref<VoxelBuffer> voxels, Array materials);

protected:
	static void _bind_methods();

	void set_padding(int minimum, int maximum);

private:
	int _minimum_padding = 0;
	int _maximum_padding = 0;
};

#endif // VOXEL_MESHER_H
