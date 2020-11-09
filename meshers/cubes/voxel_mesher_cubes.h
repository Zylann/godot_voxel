#ifndef VOXEL_MESHER_CUBES_H
#define VOXEL_MESHER_CUBES_H

#include "../voxel_mesher.h"
#include "voxel_color_palette.h"

// A super simple mesher only producing colored cubes
class VoxelMesherCubes : public VoxelMesher {
	GDCLASS(VoxelMesherCubes, VoxelMesher)
public:
	static const unsigned int PADDING = 1;

	enum Materials {
		MATERIAL_OPAQUE = 0,
		MATERIAL_TRANSPARENT,
		MATERIAL_COUNT
	};

	enum ColorMode {
		// The voxel value will be treated as an RGBA color with components of equal bit depth
		COLOR_RAW = 0,
		// The voxel value will map to a 32bit color in the palette specified on this mesher
		COLOR_MESHER_PALETTE,
		// The voxel value will be copied directly to the vertex array,
		// so the proper color can be selected by a shader.
		// LIMITATION: only one material can be used in this mode at the moment.
		COLOR_SHADER_PALETTE,

		COLOR_MODE_COUNT
	};

	VoxelMesherCubes();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	void set_greedy_meshing_enabled(bool enable);
	bool is_greedy_meshing_enabled() const;

	void set_color_mode(ColorMode mode);
	ColorMode get_color_mode() const;

	void set_palette(Ref<VoxelColorPalette> palette);
	Ref<VoxelColorPalette> get_palette() const;

	VoxelMesher *clone() override;

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
	Ref<VoxelColorPalette> _palette;
	bool _greedy_meshing = true;
	ColorMode _color_mode = COLOR_RAW;
};

VARIANT_ENUM_CAST(VoxelMesherCubes::ColorMode);
VARIANT_ENUM_CAST(VoxelMesherCubes::Materials);

#endif // VOXEL_MESHER_CUBES_H
