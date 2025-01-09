#include "inspector_property_string.h"
#include "../../errors.h"
#include "../classes/line_edit.h"

namespace zylann {

ZN_InspectorPropertyString::ZN_InspectorPropertyString() {
	_line_edit = memnew(LineEdit);
	_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	using Self = ZN_InspectorPropertyString;
	_line_edit->connect("text_submitted", callable_mp(this, &Self::on_line_edit_text_submitted));
	_line_edit->connect("text_changed", callable_mp(this, &Self::on_line_edit_text_changed));
	_line_edit->connect("focus_exited", callable_mp(this, &Self::on_line_edit_focus_exited));
	_line_edit->connect("focus_entered", callable_mp(this, &Self::on_line_edit_focus_entered));
}

void ZN_InspectorPropertyString::set_value(Variant value) {
	ZN_ASSERT_RETURN(value.get_type() == Variant::STRING);
	String text = value;
	_ignore_changes = true;
	_line_edit->set_text(text);
	_ignore_changes = false;
	_text = text;
}

Variant ZN_InspectorPropertyString::get_value() const {
	return _line_edit->get_text();
}

void ZN_InspectorPropertyString::set_enabled(bool enabled) {
	_line_edit->set_editable(enabled);
}

void ZN_InspectorPropertyString::on_line_edit_focus_entered() {
	_changed = false;
}

void ZN_InspectorPropertyString::on_line_edit_text_changed(String new_text) {
	if (_ignore_changes) {
		return;
	}
	_changed = true;
}

void ZN_InspectorPropertyString::on_line_edit_text_submitted(String text) {
	if (_ignore_changes) {
		return;
	}
	// Same behavior as the default `EditorPropertyText`
	if (_line_edit->has_focus()) {
		_line_edit->release_focus();
	}
}

void ZN_InspectorPropertyString::on_line_edit_focus_exited() {
	if (_changed) {
		_changed = false;

		String prev_text = _text;
		String text = _line_edit->get_text();

		if (prev_text != text) {
			emit_changed();
		}
	}
}

void ZN_InspectorPropertyString::_bind_methods() {
	//
}

} // namespace zylann
