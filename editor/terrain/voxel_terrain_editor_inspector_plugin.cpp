#include "voxel_terrain_editor_inspector_plugin.h"
#include "../../terrain/fixed_lod/voxel_terrain.h"
#include "../../terrain/variable_lod/voxel_lod_terrain.h"
#include "../../util/godot/classes/object.h"
#include "editor_property_aabb_min_max.h"

namespace zylann::voxel {

bool VoxelTerrainEditorInspectorPlugin::_zn_can_handle(const Object *p_object) const {
	const VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(p_object);
	if (vt != nullptr) {
		return true;
	}
	const VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(p_object);
	if (vlt != nullptr) {
		return true;
	}
	return false;
}

bool VoxelTerrainEditorInspectorPlugin::_zn_parse_property(Object *p_object, const Variant::Type p_type,
		const String &p_path, const PropertyHint p_hint, const String &p_hint_text,
		const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_type != Variant::AABB) {
		return false;
	}
	// TODO Give the same name to these properties
	if (p_path != "voxel_bounds" && p_path != "bounds") {
		return false;
	}
	// Replace default AABB editor with this one
	EditorPropertyAABBMinMax *ed = memnew(EditorPropertyAABBMinMax);
	ed->setup(-constants::MAX_VOLUME_EXTENT, constants::MAX_VOLUME_EXTENT, 1, true);
	add_property_editor(p_path, ed);
	return true;
}

} // namespace zylann::voxel
