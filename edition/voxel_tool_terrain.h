#ifndef VOXEL_TOOL_TERRAIN_H
#define VOXEL_TOOL_TERRAIN_H

#include "../util/godot/core/random_pcg.h"
#include "voxel_tool.h"

namespace zylann::voxel {

class VoxelTerrain;
class VoxelBlockyLibraryBase;
class VoxelData;

class VoxelToolTerrain : public VoxelTool {
	GDCLASS(VoxelToolTerrain, VoxelTool)
public:
	VoxelToolTerrain();
	VoxelToolTerrain(VoxelTerrain *terrain);

	bool is_area_editable(const Box3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 p_pos, Vector3 p_dir, float p_max_distance, uint32_t p_collision_mask)
			override;

	void set_voxel_metadata(Vector3i pos, Variant meta) override;
	Variant get_voxel_metadata(Vector3i pos) const override;

	void copy(Vector3i pos, VoxelBuffer &dst, uint8_t channels_mask) const override;
	void paste(Vector3i pos, const VoxelBuffer &src, uint8_t channels_mask) override;
	void paste_masked(
			Vector3i pos,
			Ref<godot::VoxelBuffer> p_voxels,
			uint8_t channels_mask,
			uint8_t mask_channel,
			uint64_t mask_value
	) override;

	void paste_masked_writable_list( //
			Vector3i pos, //
			Ref<godot::VoxelBuffer> p_voxels, //
			uint8_t channels_mask, //
			uint8_t src_mask_channel, //
			uint64_t src_mask_value, //
			uint8_t dst_mask_channel, //
			PackedInt32Array dst_writable_list //
	) override;

	void do_box(Vector3i begin, Vector3i end) override;
	void do_sphere(Vector3 center, float radius) override;
	void do_path(Span<const Vector3> positions, Span<const float> radii) override;

	// Specialized API

	void do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness);

	void run_blocky_random_tick(AABB voxel_area, int voxel_count, const Callable &callback, int block_batch_count);

	void for_each_voxel_metadata_in_area(AABB voxel_area, const Callable &callback);

protected:
	uint64_t _get_voxel(Vector3i pos) const override;
	float _get_voxel_f(Vector3i pos) const override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Box3i &box) override;

private:
	static void _bind_methods();

	VoxelTerrain *_terrain = nullptr;
	RandomPCG _random;
};

} // namespace zylann::voxel

#endif // VOXEL_TOOL_TERRAIN_H
