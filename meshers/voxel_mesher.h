#ifndef VOXEL_MESHER_H
#define VOXEL_MESHER_H

#include "../voxel_buffer.h"
#include <scene/resources/mesh.h>

class VoxelMesher : public Reference {
	GDCLASS(VoxelMesher, Reference)
public:
	struct Output {
		// Each surface correspond to a different material
		Vector<Array> surfaces;
		Mesh::PrimitiveType primitive_type;
	};

	virtual void build(Output &output, const VoxelBuffer &voxels, int padding);
	virtual int get_minimum_padding() const;

	// Must be cloneable so can be used by more than one thread
	virtual VoxelMesher *clone();

	Ref<Mesh> build_mesh(Ref<VoxelBuffer> voxels);

protected:
	static void _bind_methods();
};

#endif // VOXEL_MESHER_H
