#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../../cube_tables.h"
#include "../../util/fixed_array.h"
#include "../voxel_mesher.h"
#include <scene/resources/mesh.h>

class VoxelMesherTransvoxel : public VoxelMesher {
	GDCLASS(VoxelMesherTransvoxel, VoxelMesher)

public:
	static const int MIN_PADDING = 1;
	static const int MAX_PADDING = 2;

	VoxelMesherTransvoxel();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	VoxelMesher *clone() override;

protected:
	static void _bind_methods();

private:
	struct ReuseCell {
		FixedArray<int, 4> vertices;
	};

	struct ReuseTransitionCell {
		FixedArray<int, 12> vertices;
	};

	struct TransitionVoxels {
		const VoxelBuffer *full_resolution_neighbor_voxels[Cube::SIDE_COUNT] = { nullptr };
	};

	void build_internal(const VoxelBuffer &voxels, unsigned int channel, int lod_index);
	void build_transition(const VoxelBuffer &voxels, unsigned int channel, int direction, int lod_index);
	Ref<ArrayMesh> build_transition_mesh(Ref<VoxelBuffer> voxels, int direction);
	void reset_reuse_cells(Vector3i block_size);
	void reset_reuse_cells_2d(Vector3i block_size);
	ReuseCell &get_reuse_cell(Vector3i pos);
	ReuseTransitionCell &get_reuse_cell_2d(int x, int y);
	int emit_vertex(Vector3 primary, Vector3 normal, uint16_t border_mask, Vector3 secondary);
	void clear_output();
	void fill_surface_arrays(Array &arrays);

private:
	FixedArray<std::vector<ReuseCell>, 2> _cache;
	FixedArray<std::vector<ReuseTransitionCell>, 2> _cache_2d;
	Vector3i _block_size;

	std::vector<Vector3> _output_vertices;
	std::vector<Vector3> _output_normals;
	std::vector<Color> _output_extra;
	std::vector<int> _output_indices;
};

#endif // VOXEL_MESHER_TRANSVOXEL_H
