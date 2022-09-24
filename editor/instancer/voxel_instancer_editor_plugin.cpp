#include "voxel_instancer_editor_plugin.h"
#include "../../terrain/instancing/voxel_instancer.h"
#include "../../util/godot/callable.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/menu_button.h"
#include "../../util/godot/node.h"
#include "../../util/godot/object.h"
#include "../../util/godot/popup_menu.h"
#include "voxel_instancer_stat_view.h"

namespace zylann::voxel {

namespace {
enum MenuItemID { //
	MENU_SHOW_STATS
};
}

VoxelInstancerEditorPlugin::VoxelInstancerEditorPlugin() {
	MenuButton *menu_button = memnew(MenuButton);
	menu_button->set_text(VoxelInstancer::get_class_static());
	{
		menu_button->get_popup()->add_item("Show statistics", MENU_SHOW_STATS);
		const int i = menu_button->get_popup()->get_item_index(MENU_SHOW_STATS);
		menu_button->get_popup()->set_item_as_checkable(i, true);
		menu_button->get_popup()->set_item_checked(i, false);
	}
	menu_button->get_popup()->connect(
			"id_pressed", ZN_GODOT_CALLABLE_MP(this, VoxelInstancerEditorPlugin, _on_menu_item_selected));
	menu_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, menu_button);
	_menu_button = menu_button;
}

#if defined(ZN_GODOT)
bool VoxelInstancerEditorPlugin::handles(Object *p_object) const {
#elif defined(ZN_GODOT_EXTENSION)
bool VoxelInstancerEditorPlugin::_handles(const Variant &p_object_v) const {
	const Object *p_object = p_object_v;
#endif
	ERR_FAIL_COND_V(p_object == nullptr, false);
	return Object::cast_to<VoxelInstancer>(p_object) != nullptr;
}

#if defined(ZN_GODOT)
void VoxelInstancerEditorPlugin::edit(Object *p_object) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelInstancerEditorPlugin::_edit(const Variant &p_object_v) {
	Object *p_object = p_object_v;
#endif
	VoxelInstancer *instancer = Object::cast_to<VoxelInstancer>(p_object);
	ERR_FAIL_COND(instancer == nullptr);
	instancer->debug_set_draw_enabled(true);
	instancer->debug_set_draw_flag(VoxelInstancer::DEBUG_DRAW_ALL_BLOCKS, true);
	_node = instancer;
	if (_stat_view != nullptr) {
		_stat_view->set_instancer(_node);
	}
}

#if defined(ZN_GODOT)
void VoxelInstancerEditorPlugin::make_visible(bool visible) {
#elif defined(ZN_GODOT_EXTENSION)
void VoxelInstancerEditorPlugin::_make_visible(bool visible) {
#endif
	_menu_button->set_visible(visible);

	if (visible == false) {
		if (_node != nullptr) {
			_node->debug_set_draw_enabled(false);
			_node = nullptr;
		}
		if (_stat_view != nullptr) {
			_stat_view->hide();
			_stat_view->set_instancer(nullptr);
		}
	} else {
		if (_stat_view != nullptr) {
			_stat_view->set_instancer(_node);
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
		_stat_view->set_instancer(_node);
		_stat_view->show();

	} else {
		if (_stat_view != nullptr) {
			remove_control_from_container(CONTAINER_SPATIAL_EDITOR_BOTTOM, _stat_view);
			queue_free_node(_stat_view);
			_stat_view = nullptr;
		}
	}
	return active;
}

void VoxelInstancerEditorPlugin::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_menu_item_selected", "id"), &VoxelInstancerEditorPlugin::_on_menu_item_selected);
#endif
}

} // namespace zylann::voxel
