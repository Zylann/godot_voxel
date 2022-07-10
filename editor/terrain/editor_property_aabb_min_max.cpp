#include "editor_property_aabb_min_max.h"
#include <scene/gui/grid_container.h>

namespace zylann {

EditorPropertyAABBMinMax::EditorPropertyAABBMinMax() {
	GridContainer *grid = memnew(GridContainer);
	grid->set_columns(4);
	add_child(grid);

	for (int i = 0; i < _spin.size(); i++) {
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
		sb->connect("value_changed", callable_mp(this, &EditorPropertyAABBMinMax::_value_changed), varray(i));
		_spin[i] = sb;

		add_focusable(sb);

		grid->add_child(sb);
	}

	_spin[0]->set_label("X");
	_spin[1]->set_label("Y");
	_spin[2]->set_label("Z");
	_spin[3]->set_label("X");
	_spin[4]->set_label("Y");
	_spin[5]->set_label("Z");

	set_bottom_editor(grid);
}

void EditorPropertyAABBMinMax::_set_read_only(bool p_read_only) {
	for (int i = 0; i < 6; i++) {
		_spin[i]->set_read_only(p_read_only);
	}
};

void EditorPropertyAABBMinMax::_value_changed(double val, int spinbox_index) {
	if (_setting) {
		return;
	}

	AABB p;
	p.position.x = _spin[0]->get_value();
	p.position.y = _spin[1]->get_value();
	p.position.z = _spin[2]->get_value();
	p.size.x = _spin[3]->get_value() - p.position.x;
	p.size.y = _spin[4]->get_value() - p.position.y;
	p.size.z = _spin[5]->get_value() - p.position.z;

	emit_changed(get_edited_property(), p, "");
}

void EditorPropertyAABBMinMax::update_property() {
	const AABB val = get_edited_object()->get(get_edited_property());

	_setting = true;

	_spin[0]->set_value(val.position.x);
	_spin[1]->set_value(val.position.y);
	_spin[2]->set_value(val.position.z);
	_spin[3]->set_value(val.position.x + val.size.x);
	_spin[4]->set_value(val.position.y + val.size.y);
	_spin[5]->set_value(val.position.z + val.size.z);

	_setting = false;
}

void EditorPropertyAABBMinMax::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			const Color *colors = _get_property_colors();
			for (unsigned int i = 0; i < _spin.size(); i++) {
				_spin[i]->add_theme_color_override("label_color", colors[i % 3]);
			}
		} break;
	}
}

void EditorPropertyAABBMinMax::setup(
		double p_min, double p_max, double p_step, bool p_no_slider, const String &p_suffix) {
	for (int i = 0; i < _spin.size(); i++) {
		_spin[i]->set_min(p_min);
		_spin[i]->set_max(p_max);
		_spin[i]->set_step(p_step);
		_spin[i]->set_hide_slider(p_no_slider);
		_spin[i]->set_allow_greater(true);
		_spin[i]->set_allow_lesser(true);
		_spin[i]->set_suffix(p_suffix);
	}
}

void EditorPropertyAABBMinMax::_bind_methods() {}

} // namespace zylann
