#ifndef VOXEL_TOOL_LOD_TERRAIN_H
#define VOXEL_TOOL_LOD_TERRAIN_H

#include "voxel_tool.h"

class VoxelLodTerrain;
class VoxelMap;

class VoxelToolLodTerrain : public VoxelTool {
	GDCLASS(VoxelToolLodTerrain, VoxelTool)
public:
	VoxelToolLodTerrain(VoxelLodTerrain *terrain, Ref<VoxelMap> map);

	bool is_area_editable(const Rect3i &box) const override;

protected:
	int _get_voxel(Vector3i pos) override;
	float _get_voxel_f(Vector3i pos) override;
	void _set_voxel(Vector3i pos, int v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Rect3i &box) override;

private:
	VoxelLodTerrain *_terrain = nullptr;
	Ref<VoxelMap> _map;
};

#endif // VOXEL_TOOL_LOD_TERRAIN_H
