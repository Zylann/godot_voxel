#ifndef VOXEL_TOOL_LOD_TERRAIN_H
#define VOXEL_TOOL_LOD_TERRAIN_H

#include "../util/macros.h"
#include "voxel_tool.h"

ZN_GODOT_FORWARD_DECLARE(class Node);

namespace zylann::voxel {

class VoxelLodTerrain;
class VoxelDataMap;
class VoxelMeshSDF;
class VoxelGeneratorGraph;

class VoxelToolLodTerrain : public VoxelTool {
	GDCLASS(VoxelToolLodTerrain, VoxelTool)
public:
	VoxelToolLodTerrain() {}
	VoxelToolLodTerrain(VoxelLodTerrain *terrain);

	bool is_area_editable(const Box3i &box) const override;
	Ref<VoxelRaycastResult> raycast(Vector3 pos, Vector3 dir, float max_distance, uint32_t collision_mask) override;
	void do_sphere(Vector3 center, float radius) override;
	void copy(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) const override;
	void paste(Vector3i pos, Ref<gd::VoxelBuffer> dst, uint8_t channels_mask) override;

	// Specialized API

	int get_raycast_binary_search_iterations() const;
	void set_raycast_binary_search_iterations(int iterations);
	void do_sphere_async(Vector3 center, float radius);
	void do_hemisphere(Vector3 center, float radius, Vector3 flat_direction, float smoothness);
	float get_voxel_f_interpolated(Vector3 position) const;

	// TODO GDX: it seems binding a method taking a `Node*` fails to compile. It is supposed to be working.
#if defined(ZN_GODOT)
	Array separate_floating_chunks(AABB world_box, Node *parent_node);
#elif defined(ZN_GODOT_EXTENSION)
	Array separate_floating_chunks(AABB world_box, Object *parent_node_o);
#endif

	void stamp_sdf(Ref<VoxelMeshSDF> mesh_sdf, Transform3D transform, float isolevel, float sdf_scale);
	void do_graph(Ref<VoxelGeneratorGraph> graph, Transform3D transform, Vector3 area_size);

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

} // namespace zylann::voxel

#endif // VOXEL_TOOL_LOD_TERRAIN_H
