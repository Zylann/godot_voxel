#include "editor_property_aabb_min_max.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/grid_container.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/core/callable.h"

namespace zylann {

EditorPropertyAABBMinMax::EditorPropertyAABBMinMax() {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(4);
	add_child(grid);

	for (unsigned int i = 0; i < _spinboxes.size(); i++) {
		if (i == 0) {
			Label *label = memnew(Label);
			label->set_text("Min");
			grid->add_child(label);

		} else if (i == 3) {
			Label *label = memnew(Label);
			label->set_text("Max");
			grid->add_child(label);
		}

		EditorSpinSlider *sb = memnew(EditorSpinSlider);
		sb->set_flat(true);
		sb->set_h_size_flags(SIZE_EXPAND_FILL);
		sb->connect("value_changed", ZN_GODOT_CALLABLE_MP(this, EditorPropertyAABBMinMax, _on_value_changed));
		_spinboxes[i] = sb;

		add_focusable(sb);

		grid->add_child(sb);
	}

	_spinboxes[0]->set_label("X");
	_spinboxes[1]->set_label("Y");
	_spinboxes[2]->set_label("Z");
	_spinboxes[3]->set_label("X");
	_spinboxes[4]->set_label("Y");
	_spinboxes[5]->set_label("Z");

	set_bottom_editor(grid);
}

void EditorPropertyAABBMinMax::_zn_set_read_only(bool p_read_only) {
	for (unsigned int i = 0; i < _spinboxes.size(); i++) {
		_spinboxes[i]->set_read_only(p_read_only);
	}
};

void EditorPropertyAABBMinMax::_on_value_changed(double val) {
	if (_ignore_value_change) {
		return;
	}

	AABB p;
	p.position.x = _spinboxes[0]->get_value();
	p.position.y = _spinboxes[1]->get_value();
	p.position.z = _spinboxes[2]->get_value();
	p.size.x = _spinboxes[3]->get_value() - p.position.x;
	p.size.y = _spinboxes[4]->get_value() - p.position.y;
	p.size.z = _spinboxes[5]->get_value() - p.position.z;

	emit_changed(get_edited_property(), p, "");
}

void EditorPropertyAABBMinMax::_zn_update_property() {
	const AABB val = get_edited_object()->get(get_edited_property());

	_ignore_value_change = true;

	_spinboxes[0]->set_value(val.position.x);
	_spinboxes[1]->set_value(val.position.y);
	_spinboxes[2]->set_value(val.position.z);
	_spinboxes[3]->set_value(val.position.x + val.size.x);
	_spinboxes[4]->set_value(val.position.y + val.size.y);
	_spinboxes[5]->set_value(val.position.z + val.size.z);

	_ignore_value_change = false;
}

void EditorPropertyAABBMinMax::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			Span<const Color> colors = editor_property_get_colors(*this);
			for (unsigned int i = 0; i < _spinboxes.size(); i++) {
				_spinboxes[i]->add_theme_color_override("label_color", colors[i % 3]);
			}
		} break;
	}
}

void EditorPropertyAABBMinMax::setup(
		double p_min, double p_max, double p_step, bool p_no_slider, const String &p_suffix) {
	for (unsigned int i = 0; i < _spinboxes.size(); i++) {
		_spinboxes[i]->set_min(p_min);
		_spinboxes[i]->set_max(p_max);
		_spinboxes[i]->set_step(p_step);
		_spinboxes[i]->set_hide_slider(p_no_slider);
		_spinboxes[i]->set_allow_greater(true);
		_spinboxes[i]->set_allow_lesser(true);
		_spinboxes[i]->set_suffix(p_suffix);
	}
}

void EditorPropertyAABBMinMax::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_value_changed"), &EditorPropertyAABBMinMax::_on_value_changed);
#endif
}

} // namespace zylann
