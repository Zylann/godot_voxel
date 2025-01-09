#include "inspector_property_bool.h"
#include "../../errors.h"
#include "../classes/check_box.h"

namespace zylann {

ZN_InspectorPropertyBool::ZN_InspectorPropertyBool() {
	_check_box = memnew(CheckBox);
	_check_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_check_box->connect("toggled", callable_mp(this, &ZN_InspectorPropertyBool::on_check_box_toggled));
	add_child(_check_box);
}

void ZN_InspectorPropertyBool::set_value(Variant value) {
	ZN_ASSERT_RETURN(value.get_type() == Variant::BOOL);
	_check_box->set_pressed_no_signal(value);
}

Variant ZN_InspectorPropertyBool::get_value() const {
	return _check_box->is_pressed();
}

void ZN_InspectorPropertyBool::set_enabled(bool enabled) {
	_check_box->set_disabled(!enabled);
}

void ZN_InspectorPropertyBool::on_check_box_toggled(bool value) {
	emit_changed();
}

void ZN_InspectorPropertyBool::_bind_methods() {
	//
}

} // namespace zylann
