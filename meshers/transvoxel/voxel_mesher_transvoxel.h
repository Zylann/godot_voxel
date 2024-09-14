#ifndef VOXEL_MESHER_TRANSVOXEL_H
#define VOXEL_MESHER_TRANSVOXEL_H

#include "../../util/macros.h"
#include "../voxel_mesher.h"
#include "transvoxel.h"

ZN_GODOT_FORWARD_DECLARE(class ArrayMesh);
ZN_GODOT_FORWARD_DECLARE(class ShaderMaterial);

namespace zylann::voxel {

namespace godot {
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
	Ref<ArrayMesh> build_transition_mesh(Ref<godot::VoxelBuffer> voxels, int direction);

	int get_used_channels_mask() const override;

	bool is_generating_collision_surface() const override;

	void set_texturing_mode(TexturingMode mode);
	TexturingMode get_texturing_mode() const;

	void set_textures_ignore_air_voxels(const bool enable);
	bool get_textures_ignore_air_voxels() const;

	void set_mesh_optimization_enabled(bool enabled);
	bool is_mesh_optimization_enabled() const;

	void set_mesh_optimization_error_threshold(float threshold);
	float get_mesh_optimization_error_threshold() const;

	void set_mesh_optimization_target_ratio(float ratio);
	float get_mesh_optimization_target_ratio() const;

	void set_transitions_enabled(bool enable);
	bool get_transitions_enabled() const;

	void set_edge_clamp_margin(float margin);
	float get_edge_clamp_margin() const;

	Ref<ShaderMaterial> get_default_lod_material() const override;

	// Internal

	static void load_static_resources();
	static void free_static_resources();

	// Exposed for a fast-path. Return values are only valid until the next invocation of build() in the calling thread.
	static const transvoxel::MeshArrays &get_mesh_cache_from_current_thread();
	// Exposed for a fast-path. Return values are only valid if `detail_texture_hint` is true in the input given to
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

	// When a marching cube cell is computed, vertices may be placed anywhere on edges of the cell, including very close
	// to corners. This can lead to very thin or small triangles, which can be a problem notably for collision. this
	// margin is the minimum distance from corners, below which vertices will be clamped to it. Increasing this value
	// reduces quality of the mesh.
	float _edge_clamp_margin = 0.02f;

	bool _transitions_enabled = true;

	bool _textures_ignore_air_voxels = false;
};

} // namespace zylann::voxel

VARIANT_ENUM_CAST(zylann::voxel::VoxelMesherTransvoxel::TexturingMode);

#endif // VOXEL_MESHER_TRANSVOXEL_H
