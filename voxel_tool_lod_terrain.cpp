#include "voxel_tool_lod_terrain.h"
#include "terrain/voxel_lod_terrain.h"
#include "terrain/voxel_map.h"

VoxelToolLodTerrain::VoxelToolLodTerrain(VoxelLodTerrain *terrain, Ref<VoxelMap> map) {
	ERR_FAIL_COND(terrain == nullptr);
	_terrain = terrain;
	_map = map; // At the moment, only LOD0 is supported
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolLodTerrain::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	return _map->is_area_fully_loaded(box.padded(1));
}

int VoxelToolLodTerrain::_get_voxel(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel(pos, _channel);
}

float VoxelToolLodTerrain::_get_voxel_f(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel_f(pos, _channel);
}

void VoxelToolLodTerrain::_set_voxel(Vector3i pos, int v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel(v, pos, _channel);
}

void VoxelToolLodTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel_f(v, pos, _channel);
}

void VoxelToolLodTerrain::_post_edit(const Rect3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->post_edit_area(box);
}
