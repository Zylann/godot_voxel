#include "voxel_terrain_editor_plugin.h"
#include "../../terrain/voxel_lod_terrain.h"
#include "../../terrain/voxel_terrain.h"

VoxelTerrainEditorPlugin::VoxelTerrainEditorPlugin(EditorNode *p_node) {
	_restart_stream_button = memnew(Button);
	_restart_stream_button->set_text(TTR("Re-generate"));
	_restart_stream_button->connect("pressed", this, "_on_restart_stream_button_pressed");
	_restart_stream_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, _restart_stream_button);
}

bool VoxelTerrainEditorPlugin::handles(Object *p_object) const {
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(p_object);
	if (terrain != nullptr) {
		return true;
	}
	VoxelLodTerrain *terrain2 = Object::cast_to<VoxelLodTerrain>(p_object);
	return terrain2 != nullptr;
}

void VoxelTerrainEditorPlugin::edit(Object *p_object) {
	_node = Object::cast_to<Node>(p_object);
}

void VoxelTerrainEditorPlugin::make_visible(bool visible) {
	_restart_stream_button->set_visible(visible);

	if (!visible) {
		_node = nullptr;
	}
}

void VoxelTerrainEditorPlugin::_on_restart_stream_button_pressed() {
	ERR_FAIL_COND(_node == nullptr);
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(_node);
	if (terrain != nullptr) {
		terrain->restart_stream();
		return;
	}
	VoxelLodTerrain *terrain2 = Object::cast_to<VoxelLodTerrain>(_node);
	ERR_FAIL_COND(terrain2 == nullptr);
	terrain2->restart_stream();
}

void VoxelTerrainEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_restart_stream_button_pressed"),
			&VoxelTerrainEditorPlugin::_on_restart_stream_button_pressed);
}
