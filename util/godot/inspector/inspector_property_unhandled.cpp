#include "inspector_property_unhandled.h"
#include "../classes/label.h"
#include "../core/variant.h"

namespace zylann {

ZN_InspectorPropertyUnhandled::ZN_InspectorPropertyUnhandled() {
	_label = memnew(Label);
	add_child(_label);
}

void ZN_InspectorPropertyUnhandled::set_value(Variant value) {
	_label->set_text(value.stringify());
	_value = value;
}

Variant ZN_InspectorPropertyUnhandled::get_value() const {
	return _value;
}

void ZN_InspectorPropertyUnhandled::set_enabled(bool enabled) {
	//
}

void ZN_InspectorPropertyUnhandled::_bind_methods() {
	//
}

} // namespace zylann
