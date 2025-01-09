#include "inspector_property_enum.h"
#include "../../errors.h"
#include "../classes/option_button.h"
#include "../core/packed_arrays.h"

namespace zylann {

ZN_InspectorPropertyEnum::ZN_InspectorPropertyEnum() {
	_option_button = memnew(OptionButton);
	_option_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_option_button->connect(
			"item_selected", callable_mp(this, &ZN_InspectorPropertyEnum::on_option_button_item_selected)
	);
	add_child(_option_button);
}

void ZN_InspectorPropertyEnum::set_names(const PackedStringArray &names) {
	const Span<const String> names_s = to_span_const(names);
	_option_button->clear();
	for (const String &item_name : names_s) {
		_option_button->add_item(item_name);
	}
}

void ZN_InspectorPropertyEnum::set_value(Variant value) {
	ZN_ASSERT_RETURN(value.get_type() == Variant::INT);
	const int i = value;
	_ignore_changes = true;
	_option_button->select(i);
	_ignore_changes = false;
}

Variant ZN_InspectorPropertyEnum::get_value() const {
	return _option_button->get_selected();
}

void ZN_InspectorPropertyEnum::set_enabled(bool enabled) {
	_option_button->set_disabled(!enabled);
}

void ZN_InspectorPropertyEnum::on_option_button_item_selected(int index) {
	emit_changed();
}

void ZN_InspectorPropertyEnum::_bind_methods() {
	//
}

} // namespace zylann
