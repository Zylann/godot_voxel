#ifndef VOXEL_TOOL_TERRAIN_H
#define VOXEL_TOOL_TERRAIN_H

#include "voxel_tool.h"

class VoxelTerrain;
class VoxelMap;

class VoxelToolTerrain : public VoxelTool {
	GDCLASS(VoxelToolTerrain, VoxelTool)
public:
	VoxelToolTerrain(VoxelTerrain *terrain, Ref<VoxelMap> map);

	bool is_area_editable(const Rect3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance) override;

protected:
	int _get_voxel(Vector3i pos) override;
	float _get_voxel_f(Vector3i pos) override;
	void _set_voxel(Vector3i pos, int v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Rect3i &box) override;

private:
	VoxelTerrain *_terrain = nullptr;
	Ref<VoxelMap> _map;
};

#endif // VOXEL_TOOL_TERRAIN_H
