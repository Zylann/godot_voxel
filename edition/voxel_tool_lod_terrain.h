#ifndef VOXEL_TOOL_LOD_TERRAIN_H
#define VOXEL_TOOL_LOD_TERRAIN_H

#include "voxel_tool.h"

class VoxelLodTerrain;
class VoxelDataMap;

class VoxelToolLodTerrain : public VoxelTool {
	GDCLASS(VoxelToolLodTerrain, VoxelTool)
public:
	VoxelToolLodTerrain() {}
	VoxelToolLodTerrain(VoxelLodTerrain *terrain);

	bool is_area_editable(const Box3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) override;

	int get_raycast_binary_search_iterations() const;
	void set_raycast_binary_search_iterations(int iterations);

	void do_sphere(Vector3 center, float radius) override;
	void do_sphere_async(Vector3 center, float radius);

	void copy(Vector3i pos, Ref<VoxelBuffer> dst, uint8_t channels_mask) const override;

	// Specialized API

	float get_voxel_f_interpolated(Vector3 position) const;
	Array separate_floating_chunks(AABB world_box, Node *parent_node);

protected:
	uint64_t _get_voxel(Vector3i pos) const override;
	float _get_voxel_f(Vector3i pos) const override;
	void _set_voxel(Vector3i pos, uint64_t v) override;
	void _set_voxel_f(Vector3i pos, float v) override;
	void _post_edit(const Box3i &box) override;

private:
	static void _bind_methods();

	VoxelLodTerrain *_terrain = nullptr;
	int _raycast_binary_search_iterations = 0;
};

#endif // VOXEL_TOOL_LOD_TERRAIN_H
