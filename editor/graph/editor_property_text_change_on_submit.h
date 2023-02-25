#ifndef ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H
#define ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H

#include "../../util/godot/classes/editor_property.h"
#include "../../util/macros.h"

ZN_GODOT_FORWARD_DECLARE(class LineEdit)

namespace zylann {

// The default string editor of the inspector calls the setter of the edited object on every character typed.
// This is not always desired. Instead, this editor should emit a change only when enter is pressed, or when the
// editor looses focus.
// Note: Godot's default string editor for LineEdit is `EditorPropertyText`
class ZN_EditorPropertyTextChangeOnSubmit : public ZN_EditorProperty {
	GDCLASS(ZN_EditorPropertyTextChangeOnSubmit, ZN_EditorProperty)
public:
	ZN_EditorPropertyTextChangeOnSubmit();

protected:
	void _zn_update_property() override;

private:
	void _on_line_edit_focus_entered();
	void _on_line_edit_text_changed(String new_text);
	void _on_line_edit_text_submitted(String text);
	void _on_line_edit_focus_exited();

	static void _bind_methods();

	LineEdit *_line_edit = nullptr;
	bool _ignore_changes = false;
	bool _changed = false;
};

} // namespace zylann

#endif // ZYLANN_EDITOR_PROPERTY_TEXT_CHANGE_ON_SUBMIT_H
