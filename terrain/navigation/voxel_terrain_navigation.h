#ifndef VOXEL_TERRAIN_NAVIGATION_H
#define VOXEL_TERRAIN_NAVIGATION_H

#include "../../util/godot/classes/node_3d.h"
#include "../../util/godot/macros.h"

ZN_GODOT_FORWARD_DECLARE(class NavigationRegion3D)
ZN_GODOT_FORWARD_DECLARE(class NavigationMesh)

namespace zylann::voxel {

class VoxelTerrain;

// Bakes a single navigation mesh at runtime when terrain loads or changes.
// TODO Rename VoxelTerrainNavigationGDSingle
class VoxelTerrainNavigation : public Node3D {
	GDCLASS(VoxelTerrainNavigation, Node3D)
public:
	VoxelTerrainNavigation();

	void set_enabled(bool enabled);
	bool is_enabled() const;

	void set_template_navigation_mesh(Ref<NavigationMesh> navmesh);
	Ref<NavigationMesh> get_template_navigation_mesh() const;

	void debug_set_regions_visible_in_editor(bool enable);
	bool debug_get_regions_visible_in_editor() const;

#ifdef TOOLS_ENABLED
#if defined(ZN_GODOT)
	PackedStringArray get_configuration_warnings() const override;
#elif defined(ZN_GODOT_EXTENSION)
	PackedStringArray _get_configuration_warnings() const override;
#endif
	virtual void get_configuration_warnings(PackedStringArray &warnings) const;
#endif

protected:
	void _notification(int p_what);

private:
	void process(float delta_time);
	void bake(const VoxelTerrain &terrain);

	static void _bind_methods();

	// We only use this for baking settings, polygons in this mesh won't be used
	Ref<NavigationMesh> _template_navmesh;
	NavigationRegion3D *_navigation_region = nullptr;
	float _time_before_update = 0.f;
	float _update_period_seconds = 2.f;
	unsigned int _prev_mesh_updates_count = 0;
	bool _enabled = true;
	bool _baking = false;
	bool _visible_regions_in_editor = false;
};

} // namespace zylann::voxel

#endif // VOXEL_TERRAIN_NAVIGATION_H
