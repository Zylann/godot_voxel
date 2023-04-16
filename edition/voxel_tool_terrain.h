#ifndef VOXEL_TOOL_TERRAIN_H
#define VOXEL_TOOL_TERRAIN_H

#include "../util/godot/core/random_pcg.h"
#include "voxel_tool.h"

namespace zylann::voxel {

class VoxelTerrain;
class VoxelBlockyLibrary;
class VoxelData;

class VoxelToolTerrain : public VoxelTool {
	GDCLASS(VoxelToolTerrain, VoxelTool)
public:
	VoxelToolTerrain();
	VoxelToolTerrain(VoxelTerrain *terrain);

	bool is_area_editable(const Box3i &box) const override;
	Ref<VoxelRaycastResult> raycast(
			Vector3 p_pos, Vector3 p_dir, float p_max_distance, uint32_t p_collision_mask) override;

	void set_voxel_metadata(Vector3i pos, Variant meta) override;
	Variant get_voxel_metadata(Vector3i pos) const override;

	void copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) const override;
	void paste(Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask) override;
	void paste_masked(Vector3i pos, Ref<gd::VoxelBuffer> p_voxels, uint8_t channels_mask, uint8_t mask_channel,
			uint64_t mask_value) override;

	void do_sphere(Vector3 center, float radius) override;

	// Specialized API

	void do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness);

	void run_blocky_random_tick(AABB voxel_area, int voxel_count, const Callable &callback, int block_batch_count);

	// For easier unit testing (the regular one needs a terrain setup etc, harder to test atm)
	// The `_static` suffix is because it otherwise conflicts with the non-static method when registering the class
	static void run_blocky_random_tick_static(VoxelData &data, Box3i voxel_box, const VoxelBlockyLibrary &lib,
			RandomPCG &random, int voxel_count, int batch_count, void *callback_data,
			bool (*callback)(void *, Vector3i, int64_t));

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
