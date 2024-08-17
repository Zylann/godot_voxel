#include "voxel_instancer_editor_plugin.h"
#include "../../terrain/instancing/voxel_instancer.h"
#include "../../util/godot/classes/menu_button.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/object.h"
#include "../../util/godot/classes/popup_menu.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"
#include "../about_window.h"
#include "voxel_instancer_stat_view.h"

namespace zylann::voxel {

namespace {
enum MenuItemID { //
	MENU_SHOW_STATS,
	MENU_ABOUT
};
}

VoxelInstancerEditorPlugin::VoxelInstancerEditorPlugin() {}

// TODO GDX: Can't initialize EditorPlugins in their constructor when they access EditorNode.
// See https://github.com/godotengine/godot-cpp/issues/1179
void VoxelInstancerEditorPlugin::init() {
	MenuButton *menu_button = memnew(MenuButton);
	menu_button->set_text(VoxelInstancer::get_class_static());
	{
		PopupMenu *popup = menu_button->get_popup();
		{
			popup->add_item(ZN_TTR("Show statistics"), MENU_SHOW_STATS);
			const int i = menu_button->get_popup()->get_item_index(MENU_SHOW_STATS);
			popup->set_item_as_checkable(i, true);
			popup->set_item_checked(i, false);
		}
		{
			popup->add_separator();
			popup->add_item(ZN_TTR("About Voxel Tools..."), MENU_ABOUT);
		}
	}
	menu_button->get_popup()->connect(
			"id_pressed", callable_mp(this, &VoxelInstancerEditorPlugin::_on_menu_item_selected));
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;
}

void VoxelInstancerEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		init();
	}
}

bool VoxelInstancerEditorPlugin::_zn_handles(const Object *p_object) const {
	ERR_FAIL_COND_V(p_object == nullptr, false);
	return Object::cast_to<VoxelInstancer>(p_object) != nullptr;
}

void VoxelInstancerEditorPlugin::_zn_edit(Object *p_object) {
	// Godot will call `edit(null)` when selecting another node
	if (p_object == nullptr) {
		VoxelInstancer *instancer = get_instancer();
		if (instancer != nullptr) {
			instancer->debug_set_draw_enabled(false);
			_instancer_object_id = ObjectID();
		}
		if (_stat_view != nullptr) {
			_stat_view->set_instancer(nullptr);
		}
		return;
	}
	VoxelInstancer *instancer = Object::cast_to<VoxelInstancer>(p_object);
	ERR_FAIL_COND(instancer == nullptr);
	instancer->debug_set_draw_enabled(true);
	instancer->debug_set_draw_flag(VoxelInstancer::DEBUG_DRAW_ALL_BLOCKS, true);
	_instancer_object_id = instancer->get_instance_id();
	if (_stat_view != nullptr) {
		_stat_view->set_instancer(instancer);
	}
}

void VoxelInstancerEditorPlugin::_zn_make_visible(bool visible) {
	_menu_button->set_visible(visible);

	VoxelInstancer *instancer = get_instancer();

	if (visible == false) {
		if (instancer != nullptr) {
			instancer->debug_set_draw_enabled(false);
			_instancer_object_id = ObjectID();
		}
		if (_stat_view != nullptr) {
			_stat_view->hide();
			_stat_view->set_instancer(nullptr);
		}
	} else {
		if (_stat_view != nullptr) {
			_stat_view->set_instancer(instancer);
			_stat_view->show();
		}
	}
}

void VoxelInstancerEditorPlugin::_on_menu_item_selected(int id) {
	switch (id) {
		case MENU_SHOW_STATS: {
			const bool active = toggle_stat_view();
			const int i = _menu_button->get_popup()->get_item_index(id);
			_menu_button->get_popup()->set_item_checked(i, active);
		} break;

		case MENU_ABOUT:
			VoxelAboutWindow::popup_singleton();
			break;
	}
}

bool VoxelInstancerEditorPlugin::toggle_stat_view() {
	bool active = _stat_view == nullptr;
	if (active) {
		if (_stat_view == nullptr) {
			_stat_view = memnew(VoxelInstancerStatView);
			_stat_view->set_custom_minimum_size(Vector2(0, EDSCALE * 50));
			add_control_to_container(CONTAINER_SPATIAL_EDITOR_BOTTOM, _stat_view);
		}
		VoxelInstancer *instancer = get_instancer();
		_stat_view->set_instancer(instancer);
		_stat_view->show();

	} else {
		if (_stat_view != nullptr) {
			remove_control_from_container(CONTAINER_SPATIAL_EDITOR_BOTTOM, _stat_view);
			_stat_view->queue_free();
			_stat_view = nullptr;
		}
	}
	return active;
}

VoxelInstancer *VoxelInstancerEditorPlugin::get_instancer() {
	if (!_instancer_object_id.is_valid()) {
		return nullptr;
	}
	Object *obj = ObjectDB::get_instance(_instancer_object_id);
	if (obj == nullptr) {
		// Could have been destroyed
		return nullptr;
	}
	VoxelInstancer *instancer = Object::cast_to<VoxelInstancer>(obj);
	// We don't expect Godot to re-use the same ObjectID for different objects
	ERR_FAIL_COND_V(instancer == nullptr, nullptr);
	return instancer;
}

void VoxelInstancerEditorPlugin::_bind_methods() {}

} // namespace zylann::voxel
