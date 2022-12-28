#include "voxel_graph_editor_shader_dialog.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/code_edit.h"
#include "../../util/godot/classes/display_server.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

VoxelGraphEditorShaderDialog::VoxelGraphEditorShaderDialog() {
	set_title(ZN_TTR("Generated shader"));
	// set_resizable(true); // TODO How to set if a Window is resizable or not?
	set_min_size(Vector2(600, 300) * EDSCALE);

	VBoxContainer *v_box_container = memnew(VBoxContainer);
	v_box_container->set_anchor(SIDE_RIGHT, 1);
	v_box_container->set_anchor(SIDE_BOTTOM, 1);
	v_box_container->set_offset(SIDE_LEFT, 4 * EDSCALE);
	v_box_container->set_offset(SIDE_TOP, 4 * EDSCALE);
	v_box_container->set_offset(SIDE_RIGHT, -4 * EDSCALE);
	v_box_container->set_offset(SIDE_BOTTOM, -4 * EDSCALE);

	HBoxContainer *buttons_container = memnew(HBoxContainer);

	Button *copy_to_clipboard_button = memnew(Button);
	copy_to_clipboard_button->set_text(ZN_TTR("Copy to clipboard"));
	copy_to_clipboard_button->connect(
			"pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorShaderDialog, _on_copy_to_clipboard_button_pressed));
	buttons_container->add_child(copy_to_clipboard_button);

	v_box_container->add_child(buttons_container);

	_text_edit = memnew(CodeEdit);
	_text_edit->set_editable(false);
	_text_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_text_edit->set_draw_line_numbers(true);
	v_box_container->add_child(_text_edit);

	add_child(v_box_container);

	Button *button = get_ok_button();
	button->set_custom_minimum_size(Vector2(100 * EDSCALE, 0));
}

void VoxelGraphEditorShaderDialog::set_shader_code(const String &code) {
	_text_edit->set_text(code);
}

void VoxelGraphEditorShaderDialog::_on_copy_to_clipboard_button_pressed() {
	DisplayServer::get_singleton()->clipboard_set(_text_edit->get_text());
}

void VoxelGraphEditorShaderDialog::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_copy_to_clipboard_button_pressed"),
			&VoxelGraphEditorShaderDialog::_on_copy_to_clipboard_button_pressed);
#endif
}

} // namespace zylann::voxel
