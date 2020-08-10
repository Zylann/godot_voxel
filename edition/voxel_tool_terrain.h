#ifndef VOXEL_TOOL_TERRAIN_H
#define VOXEL_TOOL_TERRAIN_H

#include "voxel_tool.h"

class VoxelTerrain;
class VoxelMap;
class FuncRef;

class VoxelToolTerrain : public VoxelTool {
	GDCLASS(VoxelToolTerrain, VoxelTool)
public:
	VoxelToolTerrain();
	VoxelToolTerrain(VoxelTerrain *terrain, Ref<VoxelMap> map);

	bool is_area_editable(const Rect3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) override;

	void set_voxel_metadata(Vector3i pos, Variant meta) override;
	Variant get_voxel_metadata(Vector3i pos) override;

	// Specialized API

	void run_blocky_random_tick(AABB voxel_area, int voxel_count, Ref<FuncRef> callback, int block_batch_count) const;

protected:
	uint64_t _get_voxel(Vector3i pos) override;
	float _get_voxel_f(Vector3i pos) override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Rect3i &box) override;

private:
	static void _bind_methods();

	VoxelTerrain *_terrain = nullptr;
	Ref<VoxelMap> _map;
};

#endif // VOXEL_TOOL_TERRAIN_H
