#include "voxel_range_analysis_dialog.h"
#include "../../util/godot/classes/check_box.h"
#include "../../util/godot/classes/grid_container.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/spin_box.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/core/string.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

VoxelRangeAnalysisDialog::VoxelRangeAnalysisDialog() {
	set_title(ZN_TTR("Debug Range Analysis"));
	set_min_size(EDSCALE * Vector2(300, 280));

	VBoxContainer *vb = memnew(VBoxContainer);
	// vb->set_anchors_preset(Control::PRESET_TOP_WIDE);

	_enabled_checkbox = memnew(CheckBox);
	_enabled_checkbox->set_text(ZN_TTR("Enabled"));
	_enabled_checkbox->connect(
			"toggled", ZN_GODOT_CALLABLE_MP(this, VoxelRangeAnalysisDialog, _on_enabled_checkbox_toggled));
	vb->add_child(_enabled_checkbox);

	Label *tip = memnew(Label);
	// TODO Had to use `\n` and disable autowrap, otherwise the popup height becomes crazy high
	// See https://github.com/godotengine/godot/issues/47005
	tip->set_text(ZN_TTR("When enabled, hover node output labels to\ninspect their "
						 "estimated range within the\nconfigured area.\n"
						 "Nodes that may be optimized out locally will be greyed out."));
	// tip->set_autowrap(true);
	tip->set_modulate(Color(1.f, 1.f, 1.f, 0.8f));
	vb->add_child(tip);

	GridContainer *gc = memnew(GridContainer);
	gc->set_anchors_preset(Control::PRESET_TOP_WIDE);
	gc->set_columns(2);

	add_row(ZN_TTR("Position X"), _pos_x_spinbox, gc, 0);
	add_row(ZN_TTR("Position Y"), _pos_y_spinbox, gc, 0);
	add_row(ZN_TTR("Position Z"), _pos_z_spinbox, gc, 0);
	add_row(ZN_TTR("Size X"), _size_x_spinbox, gc, 100);
	add_row(ZN_TTR("Size Y"), _size_y_spinbox, gc, 100);
	add_row(ZN_TTR("Size Z"), _size_z_spinbox, gc, 100);

	vb->add_child(gc);

	add_child(vb);
}

AABB VoxelRangeAnalysisDialog::get_aabb() const {
	const Vector3 center(_pos_x_spinbox->get_value(), _pos_y_spinbox->get_value(), _pos_z_spinbox->get_value());
	const Vector3 size(_size_x_spinbox->get_value(), _size_y_spinbox->get_value(), _size_z_spinbox->get_value());
	return AABB(center - 0.5f * size, size);
}

bool VoxelRangeAnalysisDialog::is_analysis_enabled() const {
	return _enabled_checkbox->is_pressed();
}

void VoxelRangeAnalysisDialog::_on_enabled_checkbox_toggled(bool enabled) {
	emit_signal("analysis_toggled", enabled);
}

void VoxelRangeAnalysisDialog::_on_area_spinbox_value_changed(float value) {
	emit_signal("area_changed");
}

void VoxelRangeAnalysisDialog::add_row(String text, SpinBox *&sb, GridContainer *parent, float defval) {
	sb = memnew(SpinBox);
	sb->set_min(-99999.f);
	sb->set_max(99999.f);
	sb->set_value(defval);
	sb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	Label *label = memnew(Label);
	label->set_text(text);
	parent->add_child(label);
	parent->add_child(sb);
	sb->connect("value_changed", ZN_GODOT_CALLABLE_MP(this, VoxelRangeAnalysisDialog, _on_area_spinbox_value_changed));
}

void VoxelRangeAnalysisDialog::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_enabled_checkbox_toggled", "enabled"),
			&VoxelRangeAnalysisDialog::_on_enabled_checkbox_toggled);
	ClassDB::bind_method(D_METHOD("_on_area_spinbox_value_changed", "value"),
			&VoxelRangeAnalysisDialog::_on_area_spinbox_value_changed);
#endif
	ADD_SIGNAL(MethodInfo("analysis_toggled", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("area_changed"));
}

} // namespace zylann::voxel
