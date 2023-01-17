#include "voxel_graph_editor_io_dialog.h"
#include "../../generators/graph/node_type_db.h"
#include "../../util/godot/classes/button.h"
#include "../../util/godot/classes/grid_container.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/h_separator.h"
#include "../../util/godot/classes/item_list.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/line_edit.h"
#include "../../util/godot/classes/option_button.h"
#include "../../util/godot/classes/spin_box.h"
#include "../../util/godot/classes/v_box_container.h"
#include "../../util/godot/classes/v_separator.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/editor_scale.h"

namespace zylann::voxel {

using namespace pg;

VoxelGraphEditorIODialog::VoxelGraphEditorIODialog() {
	set_title(ZN_TTR("{0} inputs / outputs").format(varray(VoxelGraphFunction::get_class_static())));
	set_min_size(EDSCALE * Vector2i(600, 230));

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 4);

	_auto_generate_button = memnew(Button);
	_auto_generate_button->set_text(ZN_TTR("Auto-generate"));
	_auto_generate_button->connect(
			"pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorIODialog, _on_auto_generate_button_pressed));
	vb->add_child(_auto_generate_button);

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(create_ui(_inputs_ui, ZN_TTR("Inputs"), true));
	hb->add_child(memnew(VSeparator));
	hb->add_child(create_ui(_outputs_ui, ZN_TTR("Outputs"), false));
	vb->add_child(hb);

	vb->add_child(memnew(HSeparator));

	add_child(vb);

	get_ok_button()->connect("pressed", ZN_GODOT_CALLABLE_MP(this, VoxelGraphEditorIODialog, _on_ok_pressed));

	Button *ok_button = get_ok_button();
	ok_button->set_custom_minimum_size(Vector2(100 * EDSCALE, 0));

	Button *cancel_button = get_cancel_button();
	cancel_button->set_custom_minimum_size(Vector2(100 * EDSCALE, 0));
}

Control *VoxelGraphEditorIODialog::create_ui(PortsUI &ui, String title, bool is_input) {
	VBoxContainer *vb = memnew(VBoxContainer);

	Label *title_label = memnew(Label);
	title_label->set_text(title);
	vb->add_child(title_label);

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	ui.item_list = memnew(ItemList);
	ui.item_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	ui.item_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(ui.item_list);

	GridContainer *gc_settings = memnew(GridContainer);
	gc_settings->set_columns(2);
	{
		Label *label = memnew(Label);
		label->set_text(ZN_TTR("Name: "));
		gc_settings->add_child(label);

		ui.name = memnew(LineEdit);
		ui.name->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		gc_settings->add_child(ui.name);
	}
	{
		Label *label = memnew(Label);
		label->set_text(ZN_TTR("Usage: "));
		gc_settings->add_child(label);

		// TODO Don't allow choosing a non-custom input twice
		ui.usage = memnew(OptionButton);
		const NodeTypeDB &type_db = NodeTypeDB::get_singleton();
		const Category category = is_input ? CATEGORY_INPUT : CATEGORY_OUTPUT;
		for (unsigned int type_id = 0; type_id < VoxelGraphFunction::NODE_TYPE_COUNT; ++type_id) {
			const NodeType &ntype = type_db.get_type(type_id);
			if (ntype.category == category) {
				ui.usage->add_item(ntype.name, type_id);
			}
		}
		gc_settings->add_child(ui.usage);
	}
	if (is_input) {
		Label *label = memnew(Label);
		label->set_text(ZN_TTR("Default: "));
		gc_settings->add_child(label);

		ui.default_value = memnew(SpinBox);
		ui.default_value->set_min(-10000.0);
		ui.default_value->set_max(10000.0);
		ui.default_value->set_step(0.001);
		gc_settings->add_child(ui.default_value);
	}

	hb->add_child(gc_settings);
	vb->add_child(hb);

	// TODO Add icons

	HBoxContainer *hb_item_buttons = memnew(HBoxContainer);

	ui.add = memnew(Button);
	ui.add->set_text("+");
	hb_item_buttons->add_child(ui.add);

	ui.remove = memnew(Button);
	ui.remove->set_text("-");
	hb_item_buttons->add_child(ui.remove);

	ui.move_up = memnew(Button);
	ui.move_up->set_text("^");
	hb_item_buttons->add_child(ui.move_up);

	ui.move_down = memnew(Button);
	ui.move_down->set_text("v");
	hb_item_buttons->add_child(ui.move_down);

	vb->add_child(hb_item_buttons);
	vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	return vb;
}

void VoxelGraphEditorIODialog::set_graph(Ref<VoxelGraphFunction> graph) {
	_graph = graph;

	_inputs.clear();
	_outputs.clear();

	if (_graph.is_valid()) {
		Span<const VoxelGraphFunction::Port> inputs = _graph->get_input_definitions();
		Span<const VoxelGraphFunction::Port> outputs = _graph->get_output_definitions();

		for (const VoxelGraphFunction::Port &port : inputs) {
			_inputs.push_back(port);
		}
		for (const VoxelGraphFunction::Port &port : outputs) {
			_outputs.push_back(port);
		}
	}

	copy_data_to_ui(_inputs_ui, _inputs);
	copy_data_to_ui(_outputs_ui, _outputs);

	set_enabled(graph.is_valid());
}

void VoxelGraphEditorIODialog::set_undo_redo(EditorUndoRedoManager *undo_redo) {
	_undo_redo = undo_redo;
}

void VoxelGraphEditorIODialog::set_enabled(PortsUI &ui, bool enabled) {
	if (ui.default_value != nullptr) {
		// TODO Default values not available yet
		ui.default_value->set_editable(false);
	}

	ZN_ASSERT(ui.name != nullptr);
	ui.name->set_editable(enabled);
	ui.usage->set_disabled(!enabled);
	ui.add->set_disabled(!enabled);
	ui.remove->set_disabled(!enabled);
	ui.move_up->set_disabled(!enabled);
	ui.move_down->set_disabled(!enabled);
}

void VoxelGraphEditorIODialog::set_enabled(bool enabled) {
	_auto_generate_button->set_disabled(!enabled);
	// TODO Allow editing I/Os manually
	set_enabled(_inputs_ui, false);
	set_enabled(_outputs_ui, false);
}

void VoxelGraphEditorIODialog::clear(PortsUI &ui) {
	ui.item_list->clear();
	ui.name->clear();
}

void VoxelGraphEditorIODialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		set_process(is_visible());
		// process();
	} else if (p_what == NOTIFICATION_PROCESS) {
		process();
	}
}

// Using polling instead of signals. Seems cleaner. We'll see how it holds later.

void VoxelGraphEditorIODialog::copy_ui_to_data(const PortsUI &ui, std::vector<VoxelGraphFunction::Port> &ports) {
	const unsigned int item_count = ui.item_list->get_item_count();

	if (ports.size() != item_count) {
		ports.resize(item_count);
	}

	PackedInt32Array selection = ui.item_list->get_selected_items();

	if (selection.size() > 0) {
		const int port_index = selection[0];
		VoxelGraphFunction::Port &port = ports[port_index];

		port.name = ui.name->get_text();

		ui.item_list->set_item_text(port_index, get_port_display_name(port));

		const int usage_id = ui.usage->get_selected_id();
		ZN_ASSERT(usage_id != -1);
		port.type = VoxelGraphFunction::NodeTypeID(usage_id);
	}
}

void VoxelGraphEditorIODialog::copy_data_to_ui(PortsUI &ui, const std::vector<VoxelGraphFunction::Port> &ports) {
	if (ui.item_list->get_item_count() != int(ports.size())) {
		PackedInt32Array selection = ui.item_list->get_selected_items();

		ui.item_list->clear();
		for (const VoxelGraphFunction::Port &port : ports) {
			ui.item_list->add_item(get_port_display_name(port));
		}

		const int item_count = ui.item_list->get_item_count();
		if (item_count > 0 && selection.size() > 0) {
			const int i = math::clamp(selection[0], 0, item_count - 1);
			ui.item_list->select(i);
		}
	}

	PackedInt32Array selection = ui.item_list->get_selected_items();

	if (selection.size() > 0) {
		const int port_index = selection[0];
		const VoxelGraphFunction::Port &port = ports[port_index];

		if (ui.name->get_text() != port.name) {
			ui.name->set_text(port.name);
		}

		if (ui.usage->get_selected_id() != port.type) {
			const int usage_index = ui.usage->get_item_index(port.type);
			ZN_ASSERT(usage_index != -1);
			ui.usage->select(usage_index);
		}
	}
}

void VoxelGraphEditorIODialog::process() {
	process_ui(_inputs_ui, _inputs);
	process_ui(_outputs_ui, _outputs);
}

void VoxelGraphEditorIODialog::process_ui(PortsUI &ui, std::vector<VoxelGraphFunction::Port> &ports) {
	PackedInt32Array selection = ui.item_list->get_selected_items();
	const int port_index = selection.size() == 0 ? -1 : selection[0];
	if (port_index != ui.selected_item) {
		ui.selected_item = port_index;
		copy_data_to_ui(ui, ports);
	} else {
		copy_ui_to_data(ui, ports);
	}
}

void VoxelGraphEditorIODialog::_on_auto_generate_button_pressed() {
	ERR_FAIL_COND(_graph.is_null());

	auto_pick_input_and_outputs(_graph->get_graph(), _inputs, _outputs);

	copy_data_to_ui(_inputs_ui, _inputs);
	copy_data_to_ui(_outputs_ui, _outputs);
}

void VoxelGraphEditorIODialog::_on_ok_pressed() {
	ERR_FAIL_COND(_graph.is_null());
	ERR_FAIL_COND(_undo_redo == nullptr);

	Array previous_inputs = serialize_io_definitions(_graph->get_input_definitions());
	Array previous_outputs = serialize_io_definitions(_graph->get_output_definitions());

	Array inputs = serialize_io_definitions(to_span(_inputs));
	Array outputs = serialize_io_definitions(to_span(_outputs));

	VoxelGraphFunction *graph = _graph.ptr();

	_undo_redo->create_action(String("Set {0} I/O definitions").format(varray(VoxelGraphFunction::get_class_static())));

	_undo_redo->add_do_method(graph, "_set_input_definitions", inputs);
	_undo_redo->add_do_method(graph, "_set_output_definitions", outputs);
	_undo_redo->add_do_method(this, "reshow", _graph);

	_undo_redo->add_undo_method(graph, "_set_input_definitions", previous_inputs);
	_undo_redo->add_undo_method(graph, "_set_output_definitions", previous_outputs);
	// Show dialog again, otherwise the user can't see what undo/redo did
	_undo_redo->add_undo_method(this, "reshow", _graph);

	_undo_redo->commit_action();
}

void VoxelGraphEditorIODialog::reshow(Ref<VoxelGraphFunction> graph) {
	show();
	set_graph(graph);
}

void VoxelGraphEditorIODialog::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(
			D_METHOD("_on_auto_generate_button_pressed"), &VoxelGraphEditorIODialog::_on_auto_generate_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_ok_pressed"), &VoxelGraphEditorIODialog::_on_ok_pressed);
#endif
	ClassDB::bind_method(D_METHOD("reshow"), &VoxelGraphEditorIODialog::reshow);
}

} // namespace zylann::voxel
