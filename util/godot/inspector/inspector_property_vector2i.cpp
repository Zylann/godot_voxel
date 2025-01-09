#include "inspector_property_vector2i.h"
#include "../../errors.h"
#include "../classes/editor_spin_slider.h"
#include "../classes/label.h"

namespace zylann {

ZN_InspectorPropertyVector2i::ZN_InspectorPropertyVector2i() {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	{
		_x = memnew(EditorSpinSlider);
		_x->set_label("X");
		_x->set_step(1);
		_x->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_x->connect("value_changed", callable_mp(this, &ZN_InspectorPropertyVector2i::on_x_value_changed));
		vb->add_child(_x);
	}
	{
		_y = memnew(EditorSpinSlider);
		_y->set_label("Y");
		_y->set_step(1);
		_y->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_y->connect("value_changed", callable_mp(this, &ZN_InspectorPropertyVector2i::on_y_value_changed));
		vb->add_child(_y);
	}

	add_child(vb);
}

bool ZN_InspectorPropertyVector2i::is_wide() {
	return true;
}

void ZN_InspectorPropertyVector2i::set_range(const Rect2i rect) {
	_x->set_min(rect.position.x);
	_x->set_max(rect.position.x + rect.size.x);

	_y->set_min(rect.position.y);
	_y->set_max(rect.position.y + rect.size.y);
}

void ZN_InspectorPropertyVector2i::set_value(Variant value) {
	ZN_ASSERT_RETURN(value.get_type() == Variant::VECTOR2I);
	const Vector2i v = value;
	_x->set_value_no_signal(v.x);
	_y->set_value_no_signal(v.y);
}

Variant ZN_InspectorPropertyVector2i::get_value() const {
	return Vector2i(_x->get_value(), _y->get_value());
}

void ZN_InspectorPropertyVector2i::set_enabled(bool enabled) {
	_x->set_read_only(!enabled);
	_y->set_read_only(!enabled);
}

void ZN_InspectorPropertyVector2i::on_x_value_changed(float new_value) {
	emit_changed();
}

void ZN_InspectorPropertyVector2i::on_y_value_changed(float new_value) {
	emit_changed();
}

void ZN_InspectorPropertyVector2i::_bind_methods() {
	//
}

} // namespace zylann
