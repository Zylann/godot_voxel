#include "inspector.h"
#include "../../containers/std_vector.h"
#include "../classes/editor_spin_slider.h"
#include "../classes/h_box_container.h"
#include "../classes/label.h"
#include "../classes/v_box_container.h"
#include "inspector_property_bool.h"
#include "inspector_property_enum.h"
#include "inspector_property_string.h"
#include "inspector_property_unhandled.h"
#include "inspector_property_vector2i.h"

namespace zylann {

ZN_Inspector::ZN_Inspector() {
	set_horizontal_scroll_mode(SCROLL_MODE_DISABLED);

	_vb_container = memnew(VBoxContainer);
	_vb_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(_vb_container);
}

static void clear_children_now(Node &parent) {
	SceneTree *tree = parent.get_tree();
	ZN_ASSERT_RETURN(tree != nullptr);
	const int child_count = parent.get_child_count();
	// Iterate in reverse because it is a bit cheaper to clear node children in that order (for the same reason removing
	// items of a dynamic array is cheaper in reverse)
	for (int i = child_count - 1; i >= 0; --i) {
		Node *child = parent.get_child(i);
		parent.remove_child(child);
		tree->queue_delete(child);
	}
}

void ZN_Inspector::clear() {
	// Not using `queue_free` because when repopulating the inspector with new properties we would have old and new
	// properties all at once for a moment, until the old ones finally get freed. This makes the inspector grow in
	// size and then shrink, which can propagate to containers and have confusing effects on other controls.
	clear_children_now(*_vb_container);

	_properties.clear();
	_target_object = ObjectID();
	_target_index = -1;
}

void ZN_Inspector::set_target_object(const Object *obj) {
	ZN_ASSERT_RETURN(obj != nullptr);
	_target_object = obj->get_instance_id();
}

void ZN_Inspector::set_target_index(const int i) {
	_target_index = i;
}

void ZN_Inspector::add_indexed_property(
		const String &name,
		const StringName &setter,
		const StringName &getter,
		const Variant &hint,
		const bool editable
) {
	Object *obj = ObjectDB::get_instance(_target_object);
	ZN_ASSERT_RETURN(obj != nullptr);

	Property p;

	const Variant value = obj->call(getter, _target_index);
	switch (value.get_type()) {
		case Variant::INT: {
			switch (hint.get_type()) {
				case Variant::NIL: {
					// TODO Int properties
					p.control = memnew(ZN_InspectorPropertyUnhandled);
				} break;

				case Variant::STRING: {
					const String hint_string = hint;
					const PackedStringArray names = hint_string.split(",");
					ZN_InspectorPropertyEnum *ed = memnew(ZN_InspectorPropertyEnum);
					ed->set_names(names);
					p.control = ed;
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled hint");
					p.control = memnew(ZN_InspectorPropertyUnhandled);
					break;
			}
		} break;

		case Variant::STRING:
			p.control = memnew(ZN_InspectorPropertyString);
			break;

		case Variant::VECTOR2I: {
			switch (hint.get_type()) {
				case Variant::NIL:
					p.control = memnew(ZN_InspectorPropertyVector2i);
					break;

				case Variant::RECT2I: {
					ZN_InspectorPropertyVector2i *ed = memnew(ZN_InspectorPropertyVector2i);
					ed->set_range(hint);
					p.control = ed;
				} break;

				default:
					ZN_PRINT_ERROR("Unhandled hint");
					p.control = memnew(ZN_InspectorPropertyUnhandled);
					break;
			}
		} break;

		case Variant::BOOL:
			p.control = memnew(ZN_InspectorPropertyBool);
			break;

		default:
			p.control = memnew(ZN_InspectorPropertyUnhandled);
			break;
	}

	const unsigned int property_index = _properties.size();

	p.label = memnew(Label);
	p.label->set_vertical_alignment(VERTICAL_ALIGNMENT_TOP);
	p.label->set_text(name + ": ");

	ZN_ASSERT_RETURN(p.control != nullptr);
	p.control->set_value(value);
	p.control->set_listener(this, property_index);
	p.control->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	p.control->set_enabled(editable);

	if (p.control->is_wide()) {
		_vb_container->add_child(p.label);
		_vb_container->add_child(p.control);
	} else {
		HBoxContainer *hb = memnew(HBoxContainer);
		hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hb->add_child(p.label);
		hb->add_child(p.control);
		_vb_container->add_child(hb);
	}

	p.getter = getter;
	p.setter = setter;
	p.indexed = true;

	// const unsigned int property_index = _properties.size();
	_properties.push_back(p);
}

// void ZN_Inspector::set_property_editable(const unsigned int i, const bool editable) {
// 	ZN_ASSERT_RETURN(i >= _properties.size());
// 	Property &p = _properties[i];
// 	p.control->set_enabled(editable);
// }

void ZN_Inspector::on_property_value_changed(const unsigned int index, const Variant &value) {
	Property &p = _properties[index];
	Object *obj = ObjectDB::get_instance(_target_object);
	ZN_ASSERT_RETURN(obj != nullptr);
	if (p.indexed) {
		obj->call(p.setter, _target_index, value);
	} else {
		obj->call(p.setter, value);
	}
}

void ZN_Inspector::_bind_methods() {
	//
}

} // namespace zylann
