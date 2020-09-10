#ifndef VOXEL_MESHER_CUBES_H
#define VOXEL_MESHER_CUBES_H

#include "../voxel_mesher.h"

// A super simple mesher only producing colored cubes
class VoxelMesherCubes : public VoxelMesher {
	GDCLASS(VoxelMesherCubes, VoxelMesher)
public:
	static const unsigned int PADDING = 1;

	VoxelMesherCubes();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	void set_greedy_meshing_enabled(bool enable);
	bool is_greedy_meshing_enabled() const;

	// TODO Palette support

	VoxelMesher *clone() override;

	enum Materials {
		MATERIAL_OPAQUE = 0,
		MATERIAL_TRANSPARENT,
		MATERIAL_COUNT
	};

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3> positions;
		std::vector<Vector3> normals;
		std::vector<Color> colors;
		std::vector<int> indices;
	};

protected:
	static void _bind_methods();

private:
	FixedArray<Arrays, MATERIAL_COUNT> _arrays_per_material;
	std::vector<uint8_t> _mask_memory_pool;
	bool _greedy_meshing = true;
};

#endif // VOXEL_MESHER_CUBES_H
