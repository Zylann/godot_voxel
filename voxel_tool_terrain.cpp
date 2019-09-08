#include "voxel_tool_terrain.h"
#include "terrain/voxel_map.h"
#include "terrain/voxel_terrain.h"
#include "util/voxel_raycast.h"

VoxelToolTerrain::VoxelToolTerrain(VoxelTerrain *terrain, Ref<VoxelMap> map) {
	ERR_FAIL_COND(terrain == nullptr);
	_terrain = terrain;
	_map = map;
	// Don't destroy the terrain while a voxel tool still references it
}

bool VoxelToolTerrain::is_area_editable(const Rect3i &box) const {
	ERR_FAIL_COND_V(_terrain == nullptr, false);
	return _map->is_area_fully_loaded(box.padded(1));
}

namespace {
struct _VoxelTerrainRaycastContext {
	VoxelTerrain &terrain;
	//unsigned int channel_mask;
};
} // namespace

static bool cb_raycast_predicate(Vector3i pos, void *context_ptr) {
	// TODO This is really not the best way.
	// This needs lambda goodness and made available to other volume types (generic?)

	ERR_FAIL_COND_V(context_ptr == NULL, false);
	_VoxelTerrainRaycastContext *context = (_VoxelTerrainRaycastContext *)context_ptr;
	VoxelTerrain &terrain = context->terrain;

	//unsigned int channel = context->channel;

	Ref<VoxelMap> map = terrain.get_storage();
	int v0 = map->get_voxel(pos, Voxel::CHANNEL_TYPE);

	Ref<VoxelLibrary> lib_ref = terrain.get_voxel_library();
	if (lib_ref.is_null())
		return false;
	const VoxelLibrary &lib = **lib_ref;

	if (lib.has_voxel(v0) == false)
		return false;

	const Voxel &voxel = lib.get_voxel_const(v0);
	if (voxel.is_transparent() == false)
		return true;

	float v1 = map->get_voxel_f(pos, Voxel::CHANNEL_ISOLEVEL);
	return v1 < 0;
}

Ref<VoxelRaycastResult> VoxelToolTerrain::raycast(Vector3 pos, Vector3 dir, float max_distance) {

	// TODO Transform input if the terrain is rotated (in the future it can be made a Spatial node)

	Vector3i hit_pos;
	Vector3i prev_pos;

	_VoxelTerrainRaycastContext context = { *_terrain };
	Ref<VoxelRaycastResult> res;

	if (voxel_raycast(pos, dir, cb_raycast_predicate, &context, max_distance, hit_pos, prev_pos)) {

		res.instance();
		res->position = hit_pos;
		res->previous_position = prev_pos;
	}

	return res;
}

int VoxelToolTerrain::_get_voxel(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel(pos, _channel);
}

float VoxelToolTerrain::_get_voxel_f(Vector3i pos) {
	ERR_FAIL_COND_V(_terrain == nullptr, 0);
	return _map->get_voxel_f(pos, _channel);
}

void VoxelToolTerrain::_set_voxel(Vector3i pos, int v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel(v, pos, _channel);
}

void VoxelToolTerrain::_set_voxel_f(Vector3i pos, float v) {
	ERR_FAIL_COND(_terrain == nullptr);
	_map->set_voxel_f(v, pos, _channel);
}

void VoxelToolTerrain::_post_edit(const Rect3i &box) {
	ERR_FAIL_COND(_terrain == nullptr);
	_terrain->make_area_dirty(box);
}
