#include "voxel_terrain_editor_plugin.h"
#include "../../constants/voxel_string_names.h"
#include "../../engine/voxel_engine_gd.h"
#include "../../generators/voxel_generator.h"
#include "../../modifiers/godot/voxel_modifier_gd.h"
#include "../../terrain/fixed_lod/voxel_terrain.h"
#include "../../terrain/variable_lod/voxel_lod_terrain.h"
#include "../../util/godot/classes/camera_3d.h"
#include "../../util/godot/classes/editor_file_dialog.h"
#include "../../util/godot/classes/editor_interface.h"
#include "../../util/godot/classes/editor_settings.h"
#include "../../util/godot/classes/menu_button.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/popup_menu.h"
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
			"id_pressed", callable_mp(this, &VoxelTerrainEditorPlugin::_on_menu_item_selected));
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;

	_task_indicator = memnew(VoxelTerrainEditorTaskIndicator);
	_task_indicator->hide();
	add_control_to_container(EditorPlugin::CONTAINER_SPATIAL_EDITOR_BOTTOM, _task_indicator);

	Node *base_control = get_editor_interface()->get_base_control();

	// This plugin actually owns the singleton
	VoxelAboutWindow::create_singleton(*base_control);

	_save_file_dialog = memnew(EditorFileDialog);
	_save_file_dialog->set_title(ZN_TTR("Save Debug Terrain Dump As Scene"));
	_save_file_dialog->add_filter("*.scn", "Godot Scene File");
	_save_file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	_save_file_dialog->connect(VoxelStringNames::get_singleton().file_selected,
			callable_mp(this, &VoxelTerrainEditorPlugin::_on_save_file_dialog_file_selected));
	base_control->add_child(_save_file_dialog);
}

namespace {
void add_checkable_item(PopupMenu *popup, String text, int id, bool checked) {
	popup->add_item(text, id);
	const int i = popup->get_item_index(id);
	popup->set_item_as_checkable(i, true);
	popup->set_item_checked(i, checked);
}

} // namespace

void VoxelTerrainEditorPlugin::generate_menu_items(MenuButton *menu_button, bool is_lod_terrain) {
	PopupMenu *popup = menu_button->get_popup();
	popup->clear();

	popup->add_shortcut(zylann::godot::get_or_create_editor_shortcut("voxel/regenerate_terrain", ZN_TTR("Re-generate"),
								::godot::KEY_MASK_CMD_OR_CTRL | ::godot::KEY_R),
			MENU_RESTART_STREAM);

	popup->add_item(ZN_TTR("Re-mesh"), MENU_REMESH);
	popup->add_separator();

	add_checkable_item(
			popup, ZN_TTR("Editor Viewer Follow Camera"), MENU_STREAM_FOLLOW_CAMERA, _editor_viewer_follows_camera);
	add_checkable_item(popup, ZN_TTR("Enable Editor Viewer"), MENU_ENABLE_EDITOR_VIEWER, _editor_viewer_enabled);

	if (is_lod_terrain) {
		popup->add_separator();
		popup->add_item(ZN_TTR("Dump as scene... (Debug)"), MENU_DUMP_AS_SCENE);
	}
	popup->add_separator();
	popup->add_item(ZN_TTR("About Voxel Tools..."), MENU_ABOUT);
}

namespace {
ViewerID create_editor_viewer() {
	ViewerID id = VoxelEngine::get_singleton().add_viewer();
	VoxelEngine::Viewer::Distances vd;
	vd.horizontal = 512;
	vd.vertical = 512;
	VoxelEngine::get_singleton().set_viewer_distances(id, vd);
	// No collision needed in editor, also it updates faster without
	VoxelEngine::get_singleton().set_viewer_requires_collisions(id, false);
	return id;
}
} // namespace

void VoxelTerrainEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			init();
			if (_editor_viewer_enabled) {
				_editor_viewer_id = create_editor_viewer();
			}
			_inspector_plugin.instantiate();
			add_inspector_plugin(_inspector_plugin);
			break;

		case NOTIFICATION_EXIT_TREE:
			if (_editor_viewer_enabled) {
				VoxelEngine::get_singleton().remove_viewer(_editor_viewer_id);
			}

			remove_inspector_plugin(_inspector_plugin);

			VoxelAboutWindow::destroy_singleton();
			break;

		case NOTIFICATION_PROCESS:
			_task_indicator->update_stats();
			break;
	}
}

bool VoxelTerrainEditorPlugin::_zn_handles(const Object *p_object) const {
	return Object::cast_to<VoxelNode>(p_object) != nullptr;
}

void VoxelTerrainEditorPlugin::_zn_edit(Object *p_object) {
	VoxelNode *node = Object::cast_to<VoxelNode>(p_object);
	set_voxel_node(node);
}

void VoxelTerrainEditorPlugin::set_voxel_node(VoxelNode *node) {
	_terrain_node.set(node);

	if (node != nullptr) {
		VoxelLodTerrain *vlt = Object::cast_to<VoxelLodTerrain>(node);
		generate_menu_items(_menu_button, vlt != nullptr);
	}
}

void VoxelTerrainEditorPlugin::_zn_make_visible(bool visible) {
	_menu_button->set_visible(visible);
	_task_indicator->set_visible(visible);
	set_process(visible);

	// TODO There are deselection problems I cannot fix cleanly!

	// Can't use `make_visible(false)` to reset our reference to the node or reset gizmos,
	// because of https://github.com/godotengine/godot/issues/40166
	// So we'll need to check if _node is null all over the place
}

EditorPlugin::AfterGUIInput VoxelTerrainEditorPlugin::_zn_forward_3d_gui_input(
		Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (_editor_viewer_enabled) {
		// Will be clamped by terrain max view distance
		VoxelEngine::Viewer::Distances vd;
		vd.horizontal = p_camera->get_far();
		vd.vertical = p_camera->get_far();
		VoxelEngine::get_singleton().set_viewer_distances(_editor_viewer_id, vd);
	}
	_editor_camera_last_position = p_camera->get_global_transform().origin;

	godot::set_3d_editor_camera_cache(p_camera);

	if (_editor_viewer_follows_camera) {
		if (_editor_viewer_enabled) {
			VoxelEngine::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
		}
		godot::VoxelEngine::get_singleton()->set_editor_camera_info(
				_editor_camera_last_position, get_forward(p_camera->get_global_transform()));
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void VoxelTerrainEditorPlugin::_on_menu_item_selected(int id) {
	switch (id) {
		case MENU_RESTART_STREAM: {
			VoxelNode *node = _terrain_node.get();
			ERR_FAIL_COND(node == nullptr);
			node->restart_stream();
		} break;

		case MENU_REMESH: {
			VoxelNode *node = _terrain_node.get();
			ERR_FAIL_COND(node == nullptr);
			node->remesh_all_blocks();
		} break;

		case MENU_STREAM_FOLLOW_CAMERA: {
			_editor_viewer_follows_camera = !_editor_viewer_follows_camera;

			if (_editor_viewer_follows_camera && _editor_viewer_enabled) {
				VoxelEngine::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
			}

			const int i = _menu_button->get_popup()->get_item_index(MENU_STREAM_FOLLOW_CAMERA);
			_menu_button->get_popup()->set_item_checked(i, _editor_viewer_follows_camera);
		} break;

		case MENU_ENABLE_EDITOR_VIEWER: {
			_editor_viewer_enabled = !_editor_viewer_enabled;

			if (_editor_viewer_enabled) {
				_editor_viewer_id = create_editor_viewer();
				if (_editor_viewer_follows_camera) {
					VoxelEngine::get_singleton().set_viewer_position(_editor_viewer_id, _editor_camera_last_position);
				}
			} else {
				VoxelEngine::get_singleton().remove_viewer(_editor_viewer_id);
			}

			const int i = _menu_button->get_popup()->get_item_index(MENU_ENABLE_EDITOR_VIEWER);
			_menu_button->get_popup()->set_item_checked(i, _editor_viewer_enabled);
		} break;

		case MENU_DUMP_AS_SCENE:
			_save_file_dialog->popup_centered_ratio();
			break;

		case MENU_ABOUT:
			VoxelAboutWindow::popup_singleton();
			break;

		default: {
			ZN_ASSERT_RETURN(id >= 0 && id < MENU_COUNT);
		}
	}
}

void VoxelTerrainEditorPlugin::_on_save_file_dialog_file_selected(String fpath) {
	VoxelNode *node = _terrain_node.get();
	VoxelLodTerrain *lod_terrain = Object::cast_to<VoxelLodTerrain>(node);
	ERR_FAIL_COND(lod_terrain == nullptr);
	lod_terrain->debug_dump_as_scene(fpath, false);
}

void VoxelTerrainEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
