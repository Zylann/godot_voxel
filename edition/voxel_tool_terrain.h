#ifndef VOXEL_TOOL_TERRAIN_H
#define VOXEL_TOOL_TERRAIN_H

#include "voxel_tool.h"

class VoxelTerrain;
class FuncRef;

class VoxelToolTerrain : public VoxelTool {
	GDCLASS(VoxelToolTerrain, VoxelTool)
public:
	VoxelToolTerrain();
	VoxelToolTerrain(VoxelTerrain *terrain);

	bool is_area_editable(const Box3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 p_pos, Vector3 p_dir, float p_max_distance, uint32_t p_collision_mask) override;

	void set_voxel_metadata(Vector3i pos, Variant meta) override;
	Variant get_voxel_metadata(Vector3i pos) const override;

	void copy(Vector3i pos, Ref<VoxelBuffer> dst, uint8_t channels_mask) const override;
	void paste(Vector3i pos, Ref<VoxelBuffer> p_voxels, uint8_t channels_mask, bool use_mask, uint64_t mask_value) override;

	void do_sphere(Vector3 center, float radius) override;

	// Specialized API

	void run_blocky_random_tick(AABB voxel_area, int voxel_count, Ref<FuncRef> callback, int block_batch_count) const;
	void for_each_voxel_metadata_in_area(AABB voxel_area, Ref<FuncRef> callback);

protected:
	uint64_t _get_voxel(Vector3i pos) const override;
	float _get_voxel_f(Vector3i pos) const override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Box3i &box) override;

private:
	static void _bind_methods();

	VoxelTerrain *_terrain = nullptr;
};

#endif // VOXEL_TOOL_TERRAIN_H
