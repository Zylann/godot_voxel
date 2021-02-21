#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../../constants/cube_tables.h"
#include "../../util/fixed_array.h"
#include "../voxel_mesher.h"
#include <vector>

class VoxelMesherTransvoxelInternal {
public:
	static const int MIN_PADDING = 1;
	static const int MAX_PADDING = 2;

	struct MeshArrays {
		std::vector<Vector3> vertices;
		std::vector<Vector3> normals;
		std::vector<Color> extra;
		std::vector<int> indices;

		void clear() {
			vertices.clear();
			normals.clear();
			extra.clear();
			indices.clear();
		}
	};

	void build_internal(const VoxelBuffer &voxels, unsigned int channel, int lod_index);
	void build_transition(const VoxelBuffer &voxels, unsigned int channel, int direction, int lod_index);
	void clear_output() { _output.clear(); }
	const MeshArrays &get_output() const { return _output; }

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

	void reset_reuse_cells(Vector3i block_size);
	void reset_reuse_cells_2d(Vector3i block_size);
	ReuseCell &get_reuse_cell(Vector3i pos);
	ReuseTransitionCell &get_reuse_cell_2d(int x, int y);
	int emit_vertex(Vector3 primary, Vector3 normal, uint16_t border_mask, Vector3 secondary);

private:
	// Work cache
	FixedArray<std::vector<ReuseCell>, 2> _cache;
	FixedArray<std::vector<ReuseTransitionCell>, 2> _cache_2d;
	Vector3i _block_size;
	MeshArrays _output;
};

class ArrayMesh;

class VoxelMesherTransvoxel : public VoxelMesher {
	GDCLASS(VoxelMesherTransvoxel, VoxelMesher)

public:
	VoxelMesherTransvoxel();
	~VoxelMesherTransvoxel();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;
	Ref<ArrayMesh> build_transition_mesh(Ref<VoxelBuffer> voxels, int direction);

	Ref<Resource> duplicate(bool p_subresources = false) const override;
	int get_used_channels_mask() const override;

protected:
	static void _bind_methods();

private:
	void fill_surface_arrays(Array &arrays, const VoxelMesherTransvoxelInternal::MeshArrays &src);

	static thread_local VoxelMesherTransvoxelInternal _impl;
};

#endif // VOXEL_MESHER_TRANSVOXEL_H
