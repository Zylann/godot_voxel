#include "voxel_range_analysis_dialog.h"

#include <editor/editor_scale.h>
#include <scene/gui/check_box.h>
#include <scene/gui/grid_container.h>
#include <scene/gui/spin_box.h>

namespace zylann::voxel {

VoxelRangeAnalysisDialog::VoxelRangeAnalysisDialog() {
	set_title(TTR("Debug Range Analysis"));
	set_min_size(EDSCALE * Vector2(300, 280));

	VBoxContainer *vb = memnew(VBoxContainer);
	//vb->set_anchors_preset(Control::PRESET_TOP_WIDE);

	enabled_checkbox = memnew(CheckBox);
	enabled_checkbox->set_text(TTR("Enabled"));
	enabled_checkbox->connect("toggled", callable_mp(this, &VoxelRangeAnalysisDialog::_on_enabled_checkbox_toggled));
	vb->add_child(enabled_checkbox);

	Label *tip = memnew(Label);
	// TODO Had to use `\n` and disable autowrap, otherwise the popup height becomes crazy high
	// See https://github.com/godotengine/godot/issues/47005
	tip->set_text(TTR("When enabled, hover node output labels to\ninspect their "
					  "estimated range within the\nconfigured area.\n"
					  "Nodes that may be optimized out locally will be greyed out."));
	//tip->set_autowrap(true);
	tip->set_modulate(Color(1.f, 1.f, 1.f, 0.8f));
	vb->add_child(tip);

	GridContainer *gc = memnew(GridContainer);
	gc->set_anchors_preset(Control::PRESET_TOP_WIDE);
	gc->set_columns(2);

	add_row(TTR("Position X"), pos_x_spinbox, gc, 0);
	add_row(TTR("Position Y"), pos_y_spinbox, gc, 0);
	add_row(TTR("Position Z"), pos_z_spinbox, gc, 0);
	add_row(TTR("Size X"), size_x_spinbox, gc, 100);
	add_row(TTR("Size Y"), size_y_spinbox, gc, 100);
	add_row(TTR("Size Z"), size_z_spinbox, gc, 100);

	vb->add_child(gc);

	add_child(vb);
}

AABB VoxelRangeAnalysisDialog::get_aabb() const {
	const Vector3 center(pos_x_spinbox->get_value(), pos_y_spinbox->get_value(), pos_z_spinbox->get_value());
	const Vector3 size(size_x_spinbox->get_value(), size_y_spinbox->get_value(), size_z_spinbox->get_value());
	return AABB(center - 0.5f * size, size);
}

bool VoxelRangeAnalysisDialog::is_analysis_enabled() const {
	return enabled_checkbox->is_pressed();
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
	sb->connect("value_changed", callable_mp(this, &VoxelRangeAnalysisDialog::_on_area_spinbox_value_changed));
}

void VoxelRangeAnalysisDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("analysis_toggled", PropertyInfo(Variant::BOOL, "enabled")));
	ADD_SIGNAL(MethodInfo("area_changed"));
}

} // namespace zylann::voxel
