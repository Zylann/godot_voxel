#ifndef VOXEL_MESHER_MC_H
#define VOXEL_MESHER_MC_H

#include "../voxel_mesher.h"

// Simple marching cubes.
// Implementation is simplified from old Transvoxel code.
class VoxelMesherMC : public VoxelMesher {
	GDCLASS(VoxelMesherMC, VoxelMesher)

public:
	static const int MIN_PADDING = 1;
	static const int MAX_PADDING = 2;

	enum SeamMode {
		SEAM_NONE,
		SEAM_OVERLAP
	};

	VoxelMesherMC();

	void build(VoxelMesher::Output &output, const VoxelBuffer &voxels) override;

	void set_seam_mode(SeamMode mode);
	SeamMode get_seam_mode() const;

	VoxelMesher *clone() override;

protected:
	static void _bind_methods();

private:
	struct ReuseCell {
		int vertices[4] = { -1 };
		int case_index = 0;
	};

	void build_internal(const VoxelBuffer &voxels, unsigned int channel);
	ReuseCell &get_reuse_cell(Vector3i pos);
	void emit_vertex(Vector3 primary, Vector3 normal);

private:
	std::vector<ReuseCell> _cache[2];
	Vector3i _block_size;
	SeamMode _seam_mode = SEAM_NONE;

	std::vector<Vector3> _output_vertices;
	std::vector<Vector3> _output_normals;
	std::vector<int> _output_indices;
};

VARIANT_ENUM_CAST(VoxelMesherMC::SeamMode)

#endif // VOXEL_MESHER_MC_H
