#include "editor_plugin.h"

namespace zylann {

#if defined(ZN_GODOT)

bool ZN_EditorPlugin::handles(Object *p_object) const {
	return _zn_handles(p_object);
}

void ZN_EditorPlugin::edit(Object *p_object) {
	return _zn_edit(p_object);
}

void ZN_EditorPlugin::make_visible(bool visible) {
	return _zn_make_visible(visible);
}

EditorPlugin::AfterGUIInput ZN_EditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	return _zn_forward_3d_gui_input(p_camera, p_event);
}

void ZN_EditorPlugin::save_external_data() {
	_zn_save_external_data();
}

#elif defined(ZN_GODOT_EXTENSION)

bool ZN_EditorPlugin::_handles(Object *p_object) const {
	return _zn_handles(p_object);
}

void ZN_EditorPlugin::_edit(Object *p_object) {
	return _zn_edit(p_object);
}

void ZN_EditorPlugin::_make_visible(bool visible) {
	return _zn_make_visible(visible);
}

int32_t ZN_EditorPlugin::_forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	return _zn_forward_3d_gui_input(p_camera, p_event);
}

void ZN_EditorPlugin::_save_external_data() {
	_zn_save_external_data();
}

#endif

bool ZN_EditorPlugin::_zn_handles(const Object *p_object) const {
	return false;
}

void ZN_EditorPlugin::_zn_edit(Object *p_object) {}

void ZN_EditorPlugin::_zn_make_visible(bool visible) {}

void ZN_EditorPlugin::_zn_save_external_data() {}

EditorPlugin::AfterGUIInput ZN_EditorPlugin::_zn_forward_3d_gui_input(
		Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	return AFTER_GUI_INPUT_PASS;
}

} // namespace zylann
