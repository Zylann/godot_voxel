#ifndef VOXEL_MESHER_CUBES_H
#define VOXEL_MESHER_CUBES_H

#include "../../util/math/vector2f.h"
#include "../../util/math/vector3f.h"
#include "../../util/thread/rw_lock.h"
#include "../voxel_mesher.h"
#include "voxel_color_palette.h"

#include <vector>

namespace zylann::voxel {

// A super simple mesher only producing colored cubes
class VoxelMesherCubes : public VoxelMesher {
	GDCLASS(VoxelMesherCubes, VoxelMesher)
public:
	static const unsigned int PADDING = 1;

	enum Materials { //
		MATERIAL_OPAQUE = 0,
		MATERIAL_TRANSPARENT,
		MATERIAL_COUNT
	};

	// Tells how to interpret voxel color data
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
	~VoxelMesherCubes();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;

	void set_greedy_meshing_enabled(bool enable);
	bool is_greedy_meshing_enabled() const;

	void set_color_mode(ColorMode mode);
	ColorMode get_color_mode() const;

	void set_palette(Ref<VoxelColorPalette> palette);
	Ref<VoxelColorPalette> get_palette() const;

	Ref<Resource> duplicate(bool p_subresources = false) const ZN_OVERRIDE_UNLESS_GODOT_EXTENSION;
	int get_used_channels_mask() const override;

	void set_store_colors_in_texture(bool enable);
	bool get_store_colors_in_texture() const;

	bool supports_lod() const override {
		return true;
	}

	void set_material_by_index(Materials id, Ref<Material> material);
	Ref<Material> get_material_by_index(unsigned int i) const override;

	static Ref<Mesh> generate_mesh_from_image(Ref<Image> image, float voxel_size);

	// Structs

	// Using std::vector because they make this mesher twice as fast than Godot Vectors.
	// See why: https://github.com/godotengine/godot/issues/24731
	struct Arrays {
		std::vector<Vector3f> positions;
		std::vector<Vector3f> normals;
		std::vector<Color> colors;
		std::vector<Vector2f> uvs;
		std::vector<int> indices;

		void clear() {
			positions.clear();
			normals.clear();
			colors.clear();
			uvs.clear();
			indices.clear();
		}
	};

	struct GreedyAtlasData {
		struct ImageInfo {
			unsigned int first_color_index;
			unsigned int first_vertex_index; // From a quad
			unsigned int size_x;
			unsigned int size_y;
			unsigned int surface_index;
		};
		std::vector<Color8> colors;
		std::vector<ImageInfo> images;

		void clear() {
			colors.clear();
			images.clear();
		}
	};

private:
	void _b_set_opaque_material(Ref<Material> material);
	Ref<Material> _b_get_opaque_material() const;

	void _b_set_transparent_material(Ref<Material> material);
	Ref<Material> _b_get_transparent_material() const;

	static void _bind_methods();

	struct Parameters {
		ColorMode color_mode = COLOR_RAW;
		Ref<VoxelColorPalette> palette;
		bool greedy_meshing = true;
		bool store_colors_in_texture = false;
	};

	struct Cache {
		FixedArray<Arrays, MATERIAL_COUNT> arrays_per_material;
		std::vector<uint8_t> mask_memory_pool;
		GreedyAtlasData greedy_atlas_data;
	};

	// Parameters
	Parameters _parameters;
	RWLock _parameters_lock;

	FixedArray<Ref<Material>, MATERIAL_COUNT> _materials;

	// Work cache
	static Cache &get_tls_cache();
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherCubes::ColorMode);
VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherCubes::Materials);

#endif // VOXEL_MESHER_CUBES_H
