#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../voxel_mesher.h"
#include "transvoxel.h"

class ArrayMesh;

namespace zylann::voxel {

namespace gd {
class VoxelBuffer;
}

class VoxelMesherTransvoxel : public VoxelMesher {
	GDCLASS(VoxelMesherTransvoxel, VoxelMesher)

public:
	enum TexturingMode {
		TEXTURES_NONE = transvoxel::TEXTURES_NONE,
		TEXTURES_BLEND_4_OVER_16 = transvoxel::TEXTURES_BLEND_4_OVER_16
	};

	VoxelMesherTransvoxel();
	~VoxelMesherTransvoxel();

	void build(VoxelMesher::Output &output, const VoxelMesher::Input &input) override;
	Ref<ArrayMesh> build_transition_mesh(Ref<gd::VoxelBuffer> voxels, int direction);

	Ref<Resource> duplicate(bool p_subresources = false) const override;
	int get_used_channels_mask() const override;

	bool is_generating_collision_surface() const override;

	void set_texturing_mode(TexturingMode mode);
	TexturingMode get_texturing_mode() const;

	void set_mesh_optimization_enabled(bool enabled);
	bool is_mesh_optimization_enabled() const;

	void set_mesh_optimization_error_threshold(float threshold);
	float get_mesh_optimization_error_threshold() const;

	void set_mesh_optimization_target_ratio(float ratio);
	float get_mesh_optimization_target_ratio() const;

	void set_deep_sampling_enabled(bool enable);
	bool is_deep_sampling_enabled() const;

	void set_transitions_enabled(bool enable);
	bool get_transitions_enabled() const;

	void set_normalmap_enabled(bool enable);
	bool is_normalmap_enabled() const;

	void set_octahedral_normal_encoding(bool enable);
	bool get_octahedral_normal_encoding() const;

	void set_normalmap_tile_resolution_min(int resolution);
	int get_normalmap_tile_resolution_min() const;

	void set_normalmap_tile_resolution_max(int resolution);
	int get_normalmap_tile_resolution_max() const;

	void set_normalmap_begin_lod_index(int lod_index);
	int get_normalmap_begin_lod_index() const;

	Ref<ShaderMaterial> get_default_lod_material() const override;

	// Internal

	static void load_static_resources();
	static void free_static_resources();

	// Exposed for a fast-path. Return values are only valid until the next invocation of build() in the calling thread.
	static const transvoxel::MeshArrays &get_mesh_cache_from_current_thread();
	static const Span<const transvoxel::CellInfo> get_cell_info_from_current_thread();

	inline unsigned int get_virtual_texture_tile_resolution_for_lod(unsigned int lod_index) {
		const unsigned int relative_lod_index = lod_index - _normalmap_settings.begin_lod_index;
		const unsigned int tile_resolution =
				math::clamp(int(_normalmap_settings.tile_resolution_min << relative_lod_index),
						int(_normalmap_settings.tile_resolution_min), int(_normalmap_settings.tile_resolution_max));
		return tile_resolution;
	}

	// Not sure if that's necessary, currently transitions are either combined or not generated
	// enum TransitionMode {
	// 	// No transition meshes will be generated
	// 	TRANSITION_NONE,
	// 	// Generates transition meshes as separate meshes
	// 	TRANSITION_SEPARATE,
	// 	// Transition meshes will be part of the main mesh
	// 	TRANSITION_COMBINED
	// };

protected:
	static void _bind_methods();

private:
	TexturingMode _texture_mode = TEXTURES_NONE;

	struct MeshOptimizationParams {
		bool enabled = false;
		float error_threshold = 0.005;
		float target_ratio = 0.0;
	};

	MeshOptimizationParams _mesh_optimization_params;

	// If enabled, when meshing low-level-of-detail blocks, Transvoxel will attempt to access higher detail voxel data
	// by querying the generator and edits. This can result in better quality meshes, but is also more expensive
	// because voxel data shared between threads will have to be accessed randomly over denser data sets.
	bool _deep_sampling_enabled = false;

	bool _transitions_enabled = true;

	struct NormalMapSettings {
		// If enabled, an atlas of normalmaps will be generated for each cell of the resulting mesh, in order to add
		// more visual details using a shader.
		bool enabled = false;
		// LOD index from which normalmaps will start being generated.
		uint8_t begin_lod_index = 2;
		// Tile resolution that will be used starting from the beginning LOD. Resolution will double at each following
		// LOD index.
		uint8_t tile_resolution_min = 4;
		uint8_t tile_resolution_max = 8;
		// If enabled, encodes normalmaps using octahedral compression, which trades a bit of quality for
		// significantly reduced memory usage (using 2 bytes per pixel instead of 3).
		bool octahedral_encoding_enabled = false;
	};

	NormalMapSettings _normalmap_settings;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherTransvoxel::TexturingMode);

#endif // VOXEL_MESHER_TRANSVOXEL_H
