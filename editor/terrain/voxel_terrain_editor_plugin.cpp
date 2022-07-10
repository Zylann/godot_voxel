#include "voxel_terrain_editor_plugin.h"
#include "../../generators/voxel_generator.h"
#include "../../server/voxel_server_gd.h"
#include "../../storage/modifiers_gd.h"
#include "../../terrain/fixed_lod/voxel_terrain.h"
#include "../../terrain/variable_lod/voxel_lod_terrain.h"
#include "../about_window.h"
#include "../graph/voxel_graph_node_inspector_wrapper.h"
#include "voxel_terrain_editor_task_indicator.h"

#include <editor/editor_scale.h>
#include <scene/3d/camera_3d.h>
#include <scene/gui/menu_button.h>

namespace zylann::voxel {

VoxelTerrainEditorPlugin::VoxelTerrainEditorPlugin() {
	MenuButton *menu_button = memnew(MenuButton);
	menu_button->set_text(TTR("Terrain"));
	menu_button->get_popup()->connect(
			"id_pressed", callable_mp(this, &VoxelTerrainEditorPlugin::_on_menu_item_selected));
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;

	_task_indicator = memnew(VoxelTerrainEditorTaskIndicator);
	_task_indicator->hide();
	add_control_to_container(EditorPlugin::CONTAINER_SPATIAL_EDITOR_BOTTOM, _task_indicator);

	Node *base_control = get_editor_interface()->get_base_control();

	_about_window = memnew(VoxelAboutWindow);
	base_control->add_child(_about_window);
}

void VoxelTerrainEditorPlugin::generate_menu_items(MenuButton *menu_button, bool is_lod_terrain) {
	PopupMenu *popup = menu_button->get_popup();
	popup->clear();

	popup->add_item(TTR("Re-generate"), MENU_RESTART_STREAM);
	popup->add_item(TTR("Re-mesh"), MENU_REMESH);
	popup->add_separator();
	{
		popup->add_item(TTR("Stream follow camera"), MENU_STREAM_FOLLOW_CAMERA);
		const int i = popup->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
		popup->set_item_as_checkable(i, true);
		popup->set_item_checked(i, _editor_viewer_follows_camera);
	}
	if (is_lod_terrain) {
		popup->add_separator();
		{
			popup->add_item(TTR("Show octree bounds"), MENU_SHOW_OCTREE_BOUNDS);
			const int i = popup->get_item_index(MENU_SHOW_OCTREE_BOUNDS);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_octree_bounds);
		}
		{
			popup->add_item(TTR("Show octree nodes"), MENU_SHOW_OCTREE_NODES);
			const int i = popup->get_item_index(MENU_SHOW_OCTREE_NODES);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_octree_nodes);
		}
		{
			popup->add_item(TTR("Show mesh updates"), MENU_SHOW_MESH_UPDATES);
			const int i = popup->get_item_index(MENU_SHOW_MESH_UPDATES);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, _show_mesh_updates);
		}
	}
	popup->add_separator();
	popup->add_item(TTR("About Voxel Tools..."), MENU_ABOUT);
}

void VoxelTerrainEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			_editor_viewer_id = VoxelServer::get_singleton().add_viewer();
			VoxelServer::get_singleton().set_viewer_distance(_editor_viewer_id, 512);
			// No collision needed in editor, also it updates faster without
			VoxelServer::get_singleton().set_viewer_requires_collisions(_editor_viewer_id, false);

			_inspector_plugin.instantiate();
			add_inspector_plugin(_inspector_plugin);
			break;

		case NOTIFICATION_EXIT_TREE:
			VoxelServer::get_singleton().remove_viewer(_editor_viewer_id);
			remove_inspector_plugin(_inspector_plugin);
			break;

		case NOTIFICATION_PROCESS:
			_task_indicator->update_stats();
			break;
	}
}

// Things the plugin doesn't directly work on, but still handles to keep things visible.
// This is basically a hack because it's not easy to express that with EditorPlugin API.
// The use case being, as long as we edit an object NESTED within a voxel terrain, we should keep things visible.
static bool is_side_handled(Object *p_object) {
	// Handle stream too so we can leave some controls visible while we edit a stream or generator
	VoxelGenerator *generator = Object::cast_to<VoxelGenerator>(p_object);
	if (generator != nullptr) {
		return true;
	}
	// And have to account for this hack as well
	VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	gd::VoxelModifier *modifier = Object::cast_to<gd::VoxelModifier>(p_object);
	if (modifier != nullptr) {
		return true;
	}
	return false;
}

bool VoxelTerrainEditorPlugin::handles(Object *p_object) const {
	if (Object::cast_to<VoxelNode>(p_object) != nullptr) {
		return true;
	}
	if (_node != nullptr) {
		return is_side_handled(p_object);
	}
	return false;
}

void VoxelTerrainEditorPlugin::edit(Object *p_object) {
	VoxelNode *node = Object::cast_to<VoxelNode>(p_object);

	if (node != nullptr) {
		set_node(node);

	} else {
		if (!is_side_handled(p_object)) {
			set_node(nullptr);
		}
	}
}

void VoxelTerrainEditorPlugin::set_node(VoxelNode *node) {
	if (_node != nullptr) {
		// Using this to know when the node becomes really invalid, because ObjectID is unreliable in Godot 3.x,
		// and we may want to keep access to the node when we select some different kinds of objects.
		// Also moving the node around in the tree triggers exit/enter so have to listen for both.
		_node->disconnect("tree_entered", callable_mp(this, &VoxelTerrainEditorPlugin::_on_terrain_tree_entered));
		_node->disconnect("tree_exited", callable_mp(this, &VoxelTerrainEditorPlugin::_on_terrain_tree_exited));

		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);
		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(false);
		}
	}

	_node = node;

	if (_node != nullptr) {
		_node->connect(
				"tree_entered", callable_mp(this, &VoxelTerrainEditorPlugin::_on_terrain_tree_entered), varray(_node));
		_node->connect(
				"tree_exited", callable_mp(this, &VoxelTerrainEditorPlugin::_on_terrain_tree_exited), varray(_node));

		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);

		generate_menu_items(_menu_button, vlt != nullptr);

		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(true);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_VOLUME_BOUNDS, true);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_NODES, _show_octree_nodes);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_BOUNDS, _show_octree_bounds);
			vlt->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MESH_UPDATES, _show_mesh_updates);
		}
	}
}

void VoxelTerrainEditorPlugin::make_visible(bool visible) {
	_menu_button->set_visible(visible);
	_task_indicator->set_visible(visible);
	set_process(visible);

	if (_node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(_node);
		if (vlt != nullptr) {
			vlt->debug_set_draw_enabled(visible);
		}
	}

	// TODO There are deselection problems I cannot fix cleanly!

	// Can't use `make_visible(false)` to reset our reference to the node or reset gizmos,
	// because of https://github.com/godotengine/godot/issues/40166
	// So we'll need to check if _node is null all over the place
}

EditorPlugin::AfterGUIInput VoxelTerrainEditorPlugin::forward_spatial_gui_input(
		Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	VoxelServer::get_singleton().set_viewer_distance(_editor_viewer_id, p_camera->get_far());
	_editor_camera_last_position = p_camera->get_global_transform().origin;

	if (_editor_viewer_follows_camera) {
		VoxelServer::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
		gd::VoxelServer::get_singleton()->set_editor_camera_info(
				_editor_camera_last_position, get_forward(p_camera->get_global_transform()));
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void VoxelTerrainEditorPlugin::_on_menu_item_selected(int id) {
	switch (id) {
		case MENU_RESTART_STREAM:
			ERR_FAIL_COND(_node == nullptr);
			_node->restart_stream();
			break;

		case MENU_REMESH:
			ERR_FAIL_COND(_node == nullptr);
			_node->remesh_all_blocks();
			break;

		case MENU_STREAM_FOLLOW_CAMERA: {
			_editor_viewer_follows_camera = !_editor_viewer_follows_camera;

			if (_editor_viewer_follows_camera) {
				VoxelServer::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
			}

			const int i = _menu_button->get_popup()->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
			_menu_button->get_popup()->set_item_checked(i, _editor_viewer_follows_camera);
		} break;

		case MENU_SHOW_OCTREE_BOUNDS: {
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(_node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_octree_bounds = !_show_octree_bounds;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_BOUNDS, _show_octree_bounds);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_OCTREE_BOUNDS);
			_menu_button->get_popup()->set_item_checked(i, _show_octree_bounds);
		} break;

		case MENU_SHOW_OCTREE_NODES: {
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(_node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_octree_nodes = !_show_octree_nodes;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_OCTREE_NODES, _show_octree_nodes);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_OCTREE_NODES);
			_menu_button->get_popup()->set_item_checked(i, _show_octree_nodes);
		} break;

		case MENU_SHOW_MESH_UPDATES: {
			VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(_node);
			ERR_FAIL_COND(lod_terrain == nullptr);
			_show_mesh_updates = !_show_mesh_updates;
			lod_terrain->debug_set_draw_flag(VoxelLodTerrain::DEBUG_DRAW_MESH_UPDATES, _show_mesh_updates);

			const int i = _menu_button->get_popup()->get_item_index(MENU_SHOW_MESH_UPDATES);
			_menu_button->get_popup()->set_item_checked(i, _show_mesh_updates);
		} break;

		case MENU_ABOUT:
			_about_window->popup_centered_ratio(0.6);
			break;
	}
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_entered(Node *node) {
	_node = Object::cast_to<VoxelNode>(node);
	ERR_FAIL_COND(_node == nullptr);
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_exited(Node *node) {
	// If the node exited the tree because it was deleted, signals we connected should automatically disconnect.
	_node = nullptr;
}

void VoxelTerrainEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
