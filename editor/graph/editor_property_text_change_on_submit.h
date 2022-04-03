#ifndef ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H
#define ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H

#include <editor/editor_inspector.h>

namespace zylann {

// The default string editor of the inspector calls the setter of the edited object on every character typed.
// This is not always desired. Instead, this editor should emit a change only when enter is pressed, or when the
// editor looses focus.
// Note: Godot's default string editor for LineEdit is `EditorPropertyText`
class EditorPropertyTextChangeOnSubmit : public EditorProperty {
	GDCLASS(EditorPropertyTextChangeOnSubmit, EditorProperty)
public:
	EditorPropertyTextChangeOnSubmit() {
		_line_edit = memnew(LineEdit);
		add_child(_line_edit);
		add_focusable(_line_edit);
		_line_edit->connect(
				"text_submitted", callable_mp(this, &EditorPropertyTextChangeOnSubmit::_on_line_edit_text_submitted));
		_line_edit->connect(
				"text_changed", callable_mp(this, &EditorPropertyTextChangeOnSubmit::_on_line_edit_text_changed));
		_line_edit->connect(
				"focus_exited", callable_mp(this, &EditorPropertyTextChangeOnSubmit::_on_line_edit_focus_exited));
		_line_edit->connect(
				"focus_entered", callable_mp(this, &EditorPropertyTextChangeOnSubmit::_on_line_edit_focus_entered));
	}

	void update_property() override {
		Object *obj = get_edited_object();
		ERR_FAIL_COND(obj == nullptr);
		_ignore_changes = true;
		_line_edit->set_text(obj->get(get_edited_property()));
		_ignore_changes = false;
	}

private:
	void _on_line_edit_focus_entered() {
		_changed = false;
	}

	void _on_line_edit_text_changed(String new_text) {
		if (_ignore_changes) {
			return;
		}
		_changed = true;
	}

	void _on_line_edit_text_submitted(String text) {
		if (_ignore_changes) {
			return;
		}
		// Same behavior as the default `EditorPropertyText`
		if (_line_edit->has_focus()) {
			_line_edit->release_focus();
		}
	}

	void _on_line_edit_focus_exited() {
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

	LineEdit *_line_edit = nullptr;
	bool _ignore_changes = false;
	bool _changed = false;
};

} // namespace zylann

#endif // ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H
