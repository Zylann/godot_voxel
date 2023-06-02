#include "voxel_graph_editor_node.h"
#include "../../generators/graph/node_type_db.h"
#include "../../generators/graph/voxel_generator_graph.h"
#include "../../util/godot/classes/h_box_container.h"
#include "../../util/godot/classes/label.h"
#include "../../util/godot/classes/node.h"
#include "../../util/godot/classes/style_box_empty.h"
#include "../../util/godot/core/array.h"
#include "../../util/godot/core/callable.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/funcs.h"
#include "voxel_graph_editor_node_preview.h"

namespace zylann::voxel {

using namespace pg;

static const Color PORT_COLOR(0.4, 0.4, 1.0);

VoxelGraphEditorNode *VoxelGraphEditorNode::create(const VoxelGraphFunction &graph, uint32_t node_id) {
	VoxelGraphEditorNode *node_view = memnew(VoxelGraphEditorNode);
	node_view->set_position_offset(graph.get_node_gui_position(node_id) * EDSCALE);

	node_view->update_title(graph, node_id);

	node_view->_node_id = node_id;

	const uint32_t node_type_id = graph.get_node_type_id(node_id);
	const bool is_resizable =
			node_type_id == VoxelGraphFunction::NODE_EXPRESSION || node_type_id == VoxelGraphFunction::NODE_COMMENT;

	node_view->_is_relay = node_type_id == VoxelGraphFunction::NODE_RELAY;

	// Some nodes can have variable size title and layout. The node can get larger automatically, but doesn't shrink.
	// So for now we make them resizable so users can adjust them.
	if (is_resizable) {
		node_view->set_resizable(is_resizable);

		const Vector2 node_size = graph.get_node_gui_size(node_id) * EDSCALE;
		node_view->set_size(node_size);
	}
	// node_view.rect_size = Vector2(200, 100)

	if (node_type_id == VoxelGraphFunction::NODE_COMMENT) {
		node_view->set_comment(true);
	}

	node_view->update_layout(graph);

	if (node_type_id == VoxelGraphFunction::NODE_SDF_PREVIEW) {
		node_view->_preview = memnew(VoxelGraphEditorNodePreview);
		node_view->add_child(node_view->_preview);
	}

	return node_view;
}

void VoxelGraphEditorNode::update_layout(const VoxelGraphFunction &graph) {
	const uint32_t node_type_id = graph.get_node_type_id(_node_id);
	const NodeType &node_type = NodeTypeDB::get_singleton().get_type(node_type_id);
	// We artificially hide output ports if the node is an output.
	// These nodes have an output for implementation reasons, some outputs can process the data like any other node.
	const bool hide_outputs = node_type.category == CATEGORY_OUTPUT;

	struct Input {
		String name;
	};
	struct Output {
		String name;
	};
	std::vector<Input> inputs;
	std::vector<Output> outputs;
	{
		const unsigned int input_count = graph.get_node_input_count(_node_id);
		const unsigned int output_count = graph.get_node_output_count(_node_id);

		for (unsigned int i = 0; i < input_count; ++i) {
			Input input;
			graph.get_node_input_info(_node_id, i, &input.name, nullptr);
			inputs.push_back(input);
		}
		for (unsigned int i = 0; i < output_count; ++i) {
			Output output;
			output.name = graph.get_node_output_name(_node_id, i);
			outputs.push_back(output);
		}
	}

	const unsigned int row_count = math::max(inputs.size(), hide_outputs ? 0 : outputs.size());
	const Color hint_label_modulate(0.6, 0.6, 0.6);

	// const int middle_min_width = EDSCALE * 32.0;

	// Temporarily remove preview if any
	if (_preview != nullptr) {
		remove_child(_preview);
	}

	if (_comment_label != nullptr) {
		_comment_label->queue_free();
		_comment_label = nullptr;
	}

	// Clear previous inputs and outputs
	for (Node *row : _rows) {
		remove_child(row);
		row->queue_free();
	}
	_rows.clear();
	_output_labels.clear();

	clear_all_slots();

	_input_hints.clear();

	const bool is_relay = (node_type_id == VoxelGraphFunction::NODE_RELAY);
	// Can't remove the frame style, it breaks interaction with the node...
	// if (is_relay) {
	// 	Ref<StyleBoxEmpty> sb;
	// 	sb.instantiate();
	// 	add_theme_style_override("frame", sb);
	// } else {
	// 	remove_theme_style_override("frame");
	// }

	// Add inputs and outputs
	for (unsigned int slot_index = 0; slot_index < row_count; ++slot_index) {
		const bool has_left = slot_index < inputs.size();
		const bool has_right = (slot_index < outputs.size()) && !hide_outputs;

		HBoxContainer *property_control = memnew(HBoxContainer);
		property_control->set_custom_minimum_size(Vector2(0, 24 * EDSCALE));
		property_control->set_mouse_filter(Control::MOUSE_FILTER_PASS);

		if (has_left && !is_relay) {
			Label *label = memnew(Label);
			label->set_text(inputs[slot_index].name);
			property_control->add_child(label);

			Label *hint_label = memnew(Label);
			hint_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			hint_label->set_modulate(hint_label_modulate);
			// hint_label->set_clip_text(true);
			// hint_label->set_custom_minimum_size(Vector2(middle_min_width, 0));
			property_control->add_child(hint_label);
			VoxelGraphEditorNode::InputHint input_hint;
			input_hint.label = hint_label;
			_input_hints.push_back(input_hint);
		}

		if (has_right && !is_relay) {
			if (property_control->get_child_count() < 2) {
				Control *spacer = memnew(Control);
				spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				// spacer->set_custom_minimum_size(Vector2(middle_min_width, 0));
				property_control->add_child(spacer);
			}

			Label *label = memnew(Label);
			label->set_text(outputs[slot_index].name);
			// Pass filter is required to allow tooltips to work
			label->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			property_control->add_child(label);

			_output_labels.push_back(label);
		}

		add_child(property_control);
		set_slot(slot_index, has_left, Variant::FLOAT, PORT_COLOR, has_right, Variant::FLOAT, PORT_COLOR);
		_rows.push_back(property_control);
	}

	// Re-add preview if any
	if (_preview != nullptr) {
		add_child(_preview);
	}

	if (is_comment()) {
		_comment_label = memnew(Label);
		add_child(_comment_label);
		_comment_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_comment_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		update_comment_text(graph);
	}
}

void VoxelGraphEditorNode::update_comment_text(const VoxelGraphFunction &graph) {
	ERR_FAIL_COND(_comment_label == nullptr);
	const String text = graph.get_node_param(_node_id, 0);
	_comment_label->set_text(text);
}

void VoxelGraphEditorNode::update_title(const VoxelGraphFunction &graph) {
	update_title(graph, _node_id);
}

void VoxelGraphEditorNode::update_title(const VoxelGraphFunction &graph, uint32_t node_id) {
	const VoxelGraphFunction::NodeTypeID type_id = graph.get_node_type_id(node_id);
	if (type_id == VoxelGraphFunction::NODE_RELAY) {
		// Relays don't have title bars
		return;
	}
	const NodeType &type = NodeTypeDB::get_singleton().get_type(type_id);
	const String node_name = graph.get_node_name(node_id);

	if (type_id == VoxelGraphFunction::NODE_FUNCTION) {
		Ref<VoxelGraphFunction> func = graph.get_node_param(node_id, 0);
		ERR_FAIL_COND(func.is_null());
		String fname = func->get_path();
		fname = fname.get_file();
		if (node_name == StringName()) {
			set_title(fname);
		} else {
			set_title(String("{0} ({1})").format(varray(node_name, fname)));
		}

	} else if (node_name == StringName()) {
		set_title(type.name);

	} else if (type_id == VoxelGraphFunction::NODE_COMMENT) {
		set_title(String(node_name));

	} else {
		set_title(String("{0} ({1})").format(varray(node_name, type.name)));
	}
}

void VoxelGraphEditorNode::poll(const VoxelGraphFunction &graph) {
	poll_default_inputs(graph);
	poll_params(graph);
}

// When an input is left unconnected, it picks a default value. Input hints show this value.
// It is otherwise shown in the inspector when the node is selected, but seeing them at a glance helps.
void VoxelGraphEditorNode::poll_default_inputs(const VoxelGraphFunction &graph) {
	ProgramGraph::PortLocation src_loc_unused;
	const String prefix = ": ";

	for (unsigned int input_index = 0; input_index < _input_hints.size(); ++input_index) {
		VoxelGraphEditorNode::InputHint &input_hint = _input_hints[input_index];
		const ProgramGraph::PortLocation loc{ _node_id, input_index };

		if (graph.try_get_connection_to(loc, src_loc_unused)) {
			// There is an inbound connection, don't show the default value
			if (input_hint.last_value != Variant()) {
				input_hint.label->set_text("");
				input_hint.last_value = Variant();
			}

		} else {
			if (graph.get_node_default_inputs_autoconnect(loc.node_id)) {
				// const VoxelGraphFunction::NodeTypeID node_type_id = graph.get_node_type_id(loc.node_id);
				// const NodeType &node_type =
				// NodeTypeDB::get_singleton().get_type(node_type_id); const NodeType::Port &input_port =
				// node_type.inputs[input_index]; const VoxelGraphFunction::AutoConnect auto_connect =
				// input_port.auto_connect;
				VoxelGraphFunction::AutoConnect auto_connect;
				graph.get_node_input_info(loc.node_id, loc.port_index, nullptr, &auto_connect);
				if (auto_connect != VoxelGraphFunction::AUTO_CONNECT_NONE) {
					Variant value;
					switch (auto_connect) {
						case VoxelGraphFunction::AUTO_CONNECT_X:
							value = "Auto X";
							break;
						case VoxelGraphFunction::AUTO_CONNECT_Y:
							value = "Auto Y";
							break;
						case VoxelGraphFunction::AUTO_CONNECT_Z:
							value = "Auto Z";
							break;
						default:
							ERR_PRINT("Unhandled autoconnect");
							value = int(auto_connect);
							break;
					}
					if (input_hint.last_value != value) {
						input_hint.label->set_text(prefix + value.stringify());
						input_hint.last_value = value;
					}
					continue;
				}
			}
			// There is no inbound connection nor autoconnect, show the default value
			const Variant current_value = graph.get_node_default_input(loc.node_id, loc.port_index);
			// Only update when it changes so we don't spam editor redraws
			if (input_hint.last_value != current_value) {
				input_hint.label->set_text(prefix + String(current_value));
				input_hint.last_value = current_value;
			}
		}
	}
}

void VoxelGraphEditorNode::poll_params(const VoxelGraphFunction &graph) {
	if (graph.get_node_type_id(_node_id) == VoxelGraphFunction::NODE_EXPRESSION) {
		const String code = graph.get_node_param(_node_id, 0);
		set_title(code);
	}
}

void VoxelGraphEditorNode::update_range_analysis_tooltips(
		const VoxelGeneratorGraph &generator, const pg::Runtime::State &state) {
	for (unsigned int port_index = 0; port_index < _output_labels.size(); ++port_index) {
		ProgramGraph::PortLocation loc;
		loc.node_id = get_generator_node_id();
		loc.port_index = port_index;
		uint32_t address;
		if (!generator.try_get_output_port_address(loc, address)) {
			continue;
		}
		const math::Interval range = state.get_range(address);
		Control *label = _output_labels[port_index];
		label->set_tooltip_text(String("Min: {0}\nMax: {1}").format(varray(range.min, range.max)));
	}
}

void VoxelGraphEditorNode::clear_range_analysis_tooltips() {
	for (unsigned int i = 0; i < _output_labels.size(); ++i) {
		Control *oc = _output_labels[i];
		oc->set_tooltip_text("");
	}
}

void VoxelGraphEditorNode::set_profiling_ratio_visible(bool p_visible) {
	if (_profiling_ratio_enabled == p_visible) {
		return;
	}
	_profiling_ratio_enabled = p_visible;
	queue_redraw();
}

void VoxelGraphEditorNode::set_profiling_ratio(float ratio) {
	if (_profiling_ratio == ratio) {
		return;
	}
	_profiling_ratio = ratio;
	queue_redraw();
}

inline Color lerp(Color a, Color b, float t) {
	return Color( //
			Math::lerp(a.r, b.r, t), //
			Math::lerp(a.g, b.g, t), //
			Math::lerp(a.b, b.b, t), //
			Math::lerp(a.a, b.a, t));
}

void VoxelGraphEditorNode::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		if (_is_relay) {
			// Draw line to show that the data is directly relayed
			// TODO Thickness and antialiasing should come from GraphEdit
			const float width = Math::floor(2.f * get_theme_default_base_scale());
			// Can't directly use inputs and output positions... Godot pre-scales them, which makes them unusable
			// for drawing because the node is already scaled
			const Vector2 scale = get_global_transform().get_scale();
			const Vector2 input_pos = get_connection_input_position(0) / scale;
			const Vector2 output_pos = get_connection_output_position(0) / scale;
			draw_line(input_pos, output_pos, get_connection_input_color(0), width, true);
		}
		if (_profiling_ratio_enabled) {
			const float bgh = EDSCALE * 4.f;
			const Vector2 control_size = get_size();
			const float bgw = control_size.x;
			const Color bg_color(0.1, 0.1, 0.1);
			const Color fg_color = lerp(Color(0.8, 0.8, 0.0), Color(1.0, 0.2, 0.0), _profiling_ratio);
			draw_rect(Rect2(0, control_size.y - bgh, bgw, bgh), bg_color);
			draw_rect(Rect2(0, control_size.y - bgh, bgw * _profiling_ratio, bgh), fg_color);
		}
	}
}

void VoxelGraphEditorNode::_bind_methods() {}

} // namespace zylann::voxel
