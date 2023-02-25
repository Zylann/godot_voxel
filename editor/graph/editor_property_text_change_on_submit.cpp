#include "editor_property_text_change_on_submit.h"
#include "../../util/godot/classes/line_edit.h"
#include "../../util/godot/core/callable.h"

namespace zylann {

ZN_EditorPropertyTextChangeOnSubmit::ZN_EditorPropertyTextChangeOnSubmit() {
	_line_edit = memnew(LineEdit);
	add_child(_line_edit);
	add_focusable(_line_edit);
	_line_edit->connect("text_submitted",
			ZN_GODOT_CALLABLE_MP(this, ZN_EditorPropertyTextChangeOnSubmit, _on_line_edit_text_submitted));
	_line_edit->connect("text_changed",
			ZN_GODOT_CALLABLE_MP(this, ZN_EditorPropertyTextChangeOnSubmit, _on_line_edit_text_changed));
	_line_edit->connect("focus_exited",
			ZN_GODOT_CALLABLE_MP(this, ZN_EditorPropertyTextChangeOnSubmit, _on_line_edit_focus_exited));
	_line_edit->connect("focus_entered",
			ZN_GODOT_CALLABLE_MP(this, ZN_EditorPropertyTextChangeOnSubmit, _on_line_edit_focus_entered));
}

void ZN_EditorPropertyTextChangeOnSubmit::_zn_update_property() {
	Object *obj = get_edited_object();
	ERR_FAIL_COND(obj == nullptr);
	_ignore_changes = true;
	_line_edit->set_text(obj->get(get_edited_property()));
	_ignore_changes = false;
}

void ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_focus_entered() {
	_changed = false;
}

void ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_text_changed(String new_text) {
	if (_ignore_changes) {
		return;
	}
	_changed = true;
}

void ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_text_submitted(String text) {
	if (_ignore_changes) {
		return;
	}
	// Same behavior as the default `EditorPropertyText`
	if (_line_edit->has_focus()) {
		_line_edit->release_focus();
	}
}

void ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_focus_exited() {
	if (_changed) {
		_changed = false;

		Object *obj = get_edited_object();
		ERR_FAIL_COND(obj == nullptr);
		String prev_text = obj->get(get_edited_property());

		String text = _line_edit->get_text();

		if (prev_text != text) {
			emit_changed(get_edited_property(), text);
		}
	}
}

void ZN_EditorPropertyTextChangeOnSubmit::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_line_edit_text_submitted"),
			&ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_text_submitted);
	ClassDB::bind_method(D_METHOD("_on_line_edit_text_changed", "new_text"),
			&ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_text_changed);
	ClassDB::bind_method(D_METHOD("_on_line_edit_text_submitted", "text"),
			&ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_text_submitted);
	ClassDB::bind_method(
			D_METHOD("_on_line_edit_focus_exited"), &ZN_EditorPropertyTextChangeOnSubmit::_on_line_edit_focus_exited);
#endif
}

} // namespace zylann
