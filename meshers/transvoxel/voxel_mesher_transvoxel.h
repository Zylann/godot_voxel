#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../../util/macros.h"
#include "../voxel_mesher.h"
#include "transvoxel.h"

ZN_GODOT_FORWARD_DECLARE(class ArrayMesh);
ZN_GODOT_FORWARD_DECLARE(class ShaderMaterial);

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

	Ref<ShaderMaterial> get_default_lod_material() const override;

	// Internal

	static void load_static_resources();
	static void free_static_resources();

	// Exposed for a fast-path. Return values are only valid until the next invocation of build() in the calling thread.
	static const transvoxel::MeshArrays &get_mesh_cache_from_current_thread();
	// Exposed for a fast-path. Return values are only valid if `virtual_texture_hint` is true in the input given to
	// `build`, and only remains valid until the next invocation of build() in the calling thread.
	static Span<const transvoxel::CellInfo> get_cell_info_from_current_thread();

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
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherTransvoxel::TexturingMode);

#endif // VOXEL_MESHER_TRANSVOXEL_H
