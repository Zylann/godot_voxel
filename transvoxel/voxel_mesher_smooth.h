#ifndef VOXEL_MESHER_SMOOTH_H
#define VOXEL_MESHER_SMOOTH_H

#include "voxel_buffer.h"
#include <scene/resources/mesh.h>

class VoxelMesherSmooth : public Reference {
	GDCLASS(VoxelMesherSmooth, Reference)

public:
	VoxelMesherSmooth();

	Ref<ArrayMesh> build_mesh(Ref<VoxelBuffer> voxels_ref, unsigned int channel, Ref<ArrayMesh> mesh = Ref<ArrayMesh>());
	Array build(const VoxelBuffer &voxels, unsigned int channel);

protected:
	static void _bind_methods();

private:
	struct ReuseCell {
		int vertices[4];
		int case_index;
		ReuseCell();
	};

	void build_internal(const VoxelBuffer &voxels, unsigned int channel);
	ReuseCell &get_reuse_cell(Vector3i pos);
	void emit_vertex(Vector3 primary, Vector3 normal);

private:
	const Vector3i PAD = Vector3i(1, 1, 1);

	Vector<ReuseCell> m_cache[2];
	Vector3i m_block_size;

	Vector<Vector3> m_output_vertices;
	//Vector<Vector3> m_output_vertices_secondary;
	Vector<Vector3> m_output_normals;
	Vector<int> m_output_indices;
};

#endif // VOXEL_MESHER_SMOOTH_H
