#include "voxel_terrain_editor_plugin.h"
#include "../../engine/voxel_engine_gd.h"
#include "../../generators/voxel_generator.h"
#include "../../modifiers/godot/voxel_modifier_gd.h"
#include "../../terrain/fixed_lod/voxel_terrain.h"
#include "../../terrain/variable_lod/voxel_lod_terrain.h"
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_settings.h"
#include "../../util/godot/classes/menu_button.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/keyboard.h"
#include "../about_window.h"
#include "../camera_cache.h"
#include "../graph/voxel_graph_node_inspector_wrapper.h"
#include "voxel_terrain_editor_task_indicator.h"

namespace zylann::voxel {

VoxelTerrainEditorPlugin::VoxelTerrainEditorPlugin() {}

// TODO GDX: Can't initialize EditorPlugins in their constructor when they access EditorNode.
// See https://github.com/godotengine/godot-cpp/issues/1179
void VoxelTerrainEditorPlugin::init() {
	MenuButton *menu_button = memnew(MenuButton);
	menu_button->set_text(ZN_TTR("Terrain"));
	menu_button->get_popup()->connect(
			"id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelTerrainEditorPlugin, _on_menu_item_selected));
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;

	_task_indicator = memnew(VoxelTerrainEditorTaskIndicator);
	_task_indicator->hide();
	add_control_to_container(EditorPlugin::CONTAINER_SPATIAL_EDITOR_BOTTOM, _task_indicator);

	Node *base_control = get_editor_interface()->get_base_control();

	// This plugin actually owns the singleton
	VoxelAboutWindow::create_singleton(*base_control);
}

VoxelNode *VoxelTerrainEditorPlugin::get_voxel_node() const {
	if (!_node_object_id.is_valid()) {
		return nullptr;
	}
	Object *obj = ObjectDB::get_instance(_node_object_id);
	if (obj == nullptr) {
		// Could have been destroyed.
		// _node_object_id = ObjectID();
		return nullptr;
	}
	VoxelNode *terrain = Object::cast_to<VoxelNode>(obj);
	// We don't expect Godot to re-use the same ObjectID for different objects
	ERR_FAIL_COND_V(terrain == nullptr, nullptr);
	return terrain;
}

void VoxelTerrainEditorPlugin::generate_menu_items(MenuButton *menu_button, bool is_lod_terrain) {
	PopupMenu *popup = menu_button->get_popup();
	popup->clear();

	popup->add_shortcut(get_or_create_editor_shortcut("voxel/regenerate_terrain", ZN_TTR("Re-generate"),
								godot::KEY_MASK_CMD_OR_CTRL | godot::KEY_R),
			MENU_RESTART_STREAM);

	popup->add_item(ZN_TTR("Re-mesh"), MENU_REMESH);
	popup->add_separator();
	{
		popup->add_item(ZN_TTR("Stream follow camera"), MENU_STREAM_FOLLOW_CAMERA);
		const int i = popup->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
		popup->set_item_as_checkable(i, true);
		popup->set_item_checked(i, _editor_viewer_follows_camera);
	}
	if (is_lod_terrain) {
		popup->add_separator();
		{
			popup->add_item(ZN_TTR("Show octree bounds"), MENU_SHOW_OCTREE_BOUNDS);
			const int i = popup->get_item_index(MENU_SHOW_OCTREE_BOUNDS);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_octree_bounds);
		}
		{
			popup->add_item(ZN_TTR("Show octree nodes"), MENU_SHOW_OCTREE_NODES);
			const int i = popup->get_item_index(MENU_SHOW_OCTREE_NODES);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_octree_nodes);
		}
		{
			popup->add_item(ZN_TTR("Show mesh updates"), MENU_SHOW_MESH_UPDATES);
			const int i = popup->get_item_index(MENU_SHOW_MESH_UPDATES);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_mesh_updates);
		}
		{
			popup->add_item(ZN_TTR("Show modifier bounds"), MENU_SHOW_MODIFIER_BOUNDS);
			const int i = popup->get_item_index(MENU_SHOW_MODIFIER_BOUNDS);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_modifier_bounds);
		}
	}
	popup->add_separator();
	popup->add_item(ZN_TTR("About Voxel Tools..."), MENU_ABOUT);
}

void VoxelTerrainEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			init();

			_editor_viewer_id = VoxelEngine::get_singleton().add_viewer();
			VoxelEngine::get_singleton().set_viewer_distance(_editor_viewer_id, 512);
			// No collision needed in editor, also it updates faster without
			VoxelEngine::get_singleton().set_viewer_requires_collisions(_editor_viewer_id, false);

			_inspector_plugin.instantiate();
			add_inspector_plugin(_inspector_plugin);
			break;

		case NOTIFICATION_EXIT_TREE:
			VoxelEngine::get_singleton().remove_viewer(_editor_viewer_id);

			remove_inspector_plugin(_inspector_plugin);

			VoxelAboutWindow::destroy_singleton();
			break;

		case NOTIFICATION_PROCESS:
			_task_indicator->update_stats();
			break;
	}
}

// Things the plugin doesn't directly work on, but still handles to keep things visible.
// This is basically a hack because it's not easy to express that with EditorPlugin API.
// The use case being, as long as we edit an object NESTED within a voxel terrain, we should keep things visible.
static bool is_side_handled(const Object *p_object) {
	// Handle stream too so we can leave some controls visible while we edit a stream or generator
	const VoxelGenerator *generator = Object::cast_to<VoxelGenerator>(p_object);
	if (generator != nullptr) {
		return true;
	}
	// And have to account for this hack as well
	const VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	const gd::VoxelModifier *modifier = Object::cast_to<gd::VoxelModifier>(p_object);
	if (modifier != nullptr) {
		return true;
	}
	return false;
}

bool VoxelTerrainEditorPlugin::_zn_handles(const Object *p_object) const {
	if (Object::cast_to<VoxelNode>(p_object) != nullptr) {
		return true;
	}

	VoxelNode *node = get_voxel_node();
	if (node != nullptr) {
		return is_side_handled(p_object);
	}
	return false;
}

void VoxelTerrainEditorPlugin::_zn_edit(Object *p_object) {
	VoxelNode *node = Object::cast_to<VoxelNode>(p_object);

	if (node != nullptr) {
		set_voxel_node(node);

	} else {
		if (!is_side_handled(p_object)) {
			set_voxel_node(nullptr);
		}
	}
}

void VoxelTerrainEditorPlugin::set_voxel_node(VoxelNode *node) {
	VoxelNode *prev_node = get_voxel_node();

	if (prev_node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(prev_node);
		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(false);
		}
		VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(prev_node);
		if (vt != nullptr) {
			vt->debug_set_draw_enabled(false);
		}
	}

	_node_object_id = node != nullptr ? node->get_instance_id() : ObjectID();

	if (node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(node);
		VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(node);

		generate_menu_items(_menu_button, vlt != nullptr);

		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(true);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_VOLUME_BOUNDS, true);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_NODES, _show_octree_nodes);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_BOUNDS, _show_octree_bounds);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MESH_UPDATES, _show_mesh_updates);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MODIFIER_BOUNDS, _show_modifier_bounds);

		} else if (vt != nullptr) {
			vt->debug_set_draw_enabled(true);
			vt->debug_set_draw_flag(VoxelTerrain::DEBUG_DRAW_VOLUME_BOUNDS, true);
		}
	}
}

void VoxelTerrainEditorPlugin::_zn_make_visible(bool visible) {
	_menu_button->set_visible(visible);
	_task_indicator->set_visible(visible);
	set_process(visible);

	VoxelNode *node = get_voxel_node();

	if (node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(node);
		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(visible);
		}
		VoxelTerrain *vt = Object::cast_to<VoxelTerrain>(node);
		if (vt != nullptr) {
			vt->debug_set_draw_enabled(visible);
		}
	}

	// TODO There are deselection problems I cannot fix cleanly!

	// Can't use `make_visible(false)` to reset our reference to the node or reset gizmos,
	// because of https://github.com/godotengine/godot/issues/40166
	// So we'll need to check if _node is null all over the place
}

EditorPlugin::AfterGUIInput VoxelTerrainEditorPlugin::_zn_forward_3d_gui_input(
		Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	VoxelEngine::get_singleton().set_viewer_distance(_editor_viewer_id, p_camera->get_far());
	_editor_camera_last_position = p_camera->get_global_transform().origin;

	gd::set_3d_editor_camera_cache(p_camera);

	if (_editor_viewer_follows_camera) {
		VoxelEngine::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
		gd::VoxelEngine::get_singleton()->set_editor_camera_info(
				_editor_camera_last_position, get_forward(p_camera->get_global_transform()));
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void VoxelTerrainEditorPlugin::_on_menu_item_selected(int id) {
	switch (id) {
		case MENU_RESTART_STREAM: {
			VoxelNode *node = get_voxel_node();
			ERR_FAIL_COND(node == nullptr);
			node->restart_stream();
		} break;

		case MENU_REMESH: {
			VoxelNode *node = get_voxel_node();
			ERR_FAIL_COND(node == nullptr);
			node->remesh_all_blocks();
		} break;

		case MENU_STREAM_FOLLOW_CAMERA: {
			_editor_viewer_follows_camera = !_editor_viewer_follows_camera;

			if (_editor_viewer_follows_camera) {
				VoxelEngine::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
			}

			const int i = _menu_button->get_popup()->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
			_menu_button->get_popup()->set_item_checked(i, _editor_viewer_follows_camera);
		} break;

		case MENU_SHOW_OCTREE_BOUNDS: {
			VoxelNode *node = get_voxel_node();
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_octree_bounds = !_show_octree_bounds;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_BOUNDS, _show_octree_bounds);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_OCTREE_BOUNDS);
			_menu_button->get_popup()->set_item_checked(i, _show_octree_bounds);
		} break;

		case MENU_SHOW_OCTREE_NODES: {
			VoxelNode *node = get_voxel_node();
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_octree_nodes = !_show_octree_nodes;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_NODES, _show_octree_nodes);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_OCTREE_NODES);
			_menu_button->get_popup()->set_item_checked(i, _show_octree_nodes);
		} break;

		case MENU_SHOW_MESH_UPDATES: {
			VoxelNode *node = get_voxel_node();
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_mesh_updates = !_show_mesh_updates;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MESH_UPDATES, _show_mesh_updates);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_MESH_UPDATES);
			_menu_button->get_popup()->set_item_checked(i, _show_mesh_updates);
		} break;

		case MENU_SHOW_MODIFIER_BOUNDS: {
			VoxelNode *node = get_voxel_node();
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_modifier_bounds = !_show_modifier_bounds;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MODIFIER_BOUNDS, _show_modifier_bounds);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_MODIFIER_BOUNDS);
			_menu_button->get_popup()->set_item_checked(i, _show_modifier_bounds);
		} break;

		case MENU_ABOUT:
			VoxelAboutWindow::popup_singleton();
			break;
	}
}

void VoxelTerrainEditorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_menu_item_selected", "id"), &VoxelTerrainEditorPlugin::_on_menu_item_selected);
#endif
}

} // namespace zylann::voxel
