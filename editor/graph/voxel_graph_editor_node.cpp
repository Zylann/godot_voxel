#include "voxel_graph_editor_node.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../util/godot/array.h"
#include "../../util/godot/callable.h"
#include "../../util/godot/editor_scale.h"
#include "../../util/godot/funcs.h"
#include "../../util/godot/h_box_container.h"
#include "../../util/godot/label.h"
#include "../../util/godot/node.h"
#include "voxel_graph_editor_node_preview.h"

namespace zylann::voxel {

VoxelGraphEditorNode *VoxelGraphEditorNode::create(const VoxelGeneratorGraph &graph, uint32_t node_id) {
	VoxelGraphEditorNode *node_view = memnew(VoxelGraphEditorNode);
	node_view->set_position_offset(graph.get_node_gui_position(node_id) * EDSCALE);

	const uint32_t node_type_id = graph.get_node_type_id(node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(node_type_id);

	const StringName node_name = graph.get_node_name(node_id);
	node_view->update_title(node_name, node_type.name);

	node_view->_node_id = node_id;
	// Some nodes can have variable size title and layout. The node can get larger automatically, but doesn't shrink.
	// So for now we make them resizable so users can adjust them.
	node_view->set_resizable(node_type_id == VoxelGeneratorGraph::NODE_EXPRESSION);
	//node_view.rect_size = Vector2(200, 100)

	node_view->update_layout(graph);

	if (node_type_id == VoxelGeneratorGraph::NODE_SDF_PREVIEW) {
		node_view->_preview = memnew(VoxelGraphEditorNodePreview);
		node_view->add_child(node_view->_preview);
	}

	if (node_view->is_resizable()) {
		node_view->connect("resize_request", ZN_GODOT_CALLABLE_MP(node_view, VoxelGraphEditorNode, _on_resize_request));
	}

	return node_view;
}

void VoxelGraphEditorNode::_on_resize_request(Vector2 new_size) {
	set_size(new_size);
}

void VoxelGraphEditorNode::update_layout(const VoxelGeneratorGraph &graph) {
	const uint32_t node_type_id = graph.get_node_type_id(_node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(node_type_id);
	// We artificially hide output ports if the node is an output.
	// These nodes have an output for implementation reasons, some outputs can process the data like any other node.
	const bool hide_outputs = node_type.category == VoxelGraphNodeDB::CATEGORY_OUTPUT;

	struct Input {
		String name;
	};
	struct Output {
		String name;
	};
	std::vector<Input> inputs;
	std::vector<Output> outputs;
	{
		for (const VoxelGraphNodeDB::Port &port : node_type.inputs) {
			inputs.push_back({ port.name });
		}
		for (const VoxelGraphNodeDB::Port &port : node_type.outputs) {
			outputs.push_back({ port.name });
		}
		if (graph.get_node_type_id(_node_id) == VoxelGeneratorGraph::NODE_EXPRESSION) {
			std::vector<std::string> names;
			graph.get_expression_node_inputs(_node_id, names);
			for (const std::string &s : names) {
				inputs.push_back({ to_godot(s) });
			}
		}
	}

	const unsigned int row_count = math::max(inputs.size(), hide_outputs ? 0 : outputs.size());
	const Color port_color(0.4, 0.4, 1.0);
	const Color hint_label_modulate(0.6, 0.6, 0.6);

	//const int middle_min_width = EDSCALE * 32.0;

	// Temporarily remove preview if any
	if (_preview != nullptr) {
		remove_child(_preview);
	}

	// Clear previous inputs and outputs
	for (Node *row : _rows) {
		remove_child(row);
		queue_free_node(row);
	}
	_rows.clear();

	clear_all_slots();

	_input_hints.clear();

	// Add inputs and outputs
	for (unsigned int slot_index = 0; slot_index < row_count; ++slot_index) {
		const bool has_left = slot_index < inputs.size();
		const bool has_right = (slot_index < outputs.size()) && !hide_outputs;

		HBoxContainer *property_control = memnew(HBoxContainer);
		property_control->set_custom_minimum_size(Vector2(0, 24 * EDSCALE));

		if (has_left) {
			Label *label = memnew(Label);
			label->set_text(inputs[slot_index].name);
			property_control->add_child(label);

			Label *hint_label = memnew(Label);
			hint_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			hint_label->set_modulate(hint_label_modulate);
			//hint_label->set_clip_text(true);
			//hint_label->set_custom_minimum_size(Vector2(middle_min_width, 0));
			property_control->add_child(hint_label);
			VoxelGraphEditorNode::InputHint input_hint;
			input_hint.label = hint_label;
			_input_hints.push_back(input_hint);
		}

		if (has_right) {
			if (property_control->get_child_count() < 2) {
				Control *spacer = memnew(Control);
				spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
				//spacer->set_custom_minimum_size(Vector2(middle_min_width, 0));
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
		set_slot(slot_index, has_left, Variant::FLOAT, port_color, has_right, Variant::FLOAT, port_color);
		_rows.push_back(property_control);
	}

	// Re-add preview if any
	if (_preview != nullptr) {
		add_child(_preview);
	}
}

void VoxelGraphEditorNode::update_title(StringName node_name, String node_type_name) {
	if (node_name == StringName()) {
		set_title(node_type_name);
	} else {
		set_title(String("{0} ({1})").format(varray(node_name, node_type_name)));
	}
}

void VoxelGraphEditorNode::poll(const VoxelGeneratorGraph &graph) {
	poll_default_inputs(graph);
	poll_params(graph);
}

// When an input is left unconnected, it picks a default value. Input hints show this value.
// It is otherwise shown in the inspector when the node is selected, but seeing them at a glance helps.
void VoxelGraphEditorNode::poll_default_inputs(const VoxelGeneratorGraph &graph) {
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
				const VoxelGeneratorGraph::NodeTypeID node_type_id = graph.get_node_type_id(loc.node_id);
				const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton().get_type(node_type_id);
				const VoxelGraphNodeDB::Port &input_port = node_type.inputs[input_index];
				if (input_port.auto_connect != VoxelGraphNodeDB::AUTO_CONNECT_NONE) {
					Variant value;
					switch (input_port.auto_connect) {
						case VoxelGraphNodeDB::AUTO_CONNECT_X:
							value = "Auto X";
							break;
						case VoxelGraphNodeDB::AUTO_CONNECT_Y:
							value = "Auto Y";
							break;
						case VoxelGraphNodeDB::AUTO_CONNECT_Z:
							value = "Auto Z";
							break;
						default:
							ERR_PRINT("Unhandled autoconnect");
							value = int(input_port.auto_connect);
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

void VoxelGraphEditorNode::poll_params(const VoxelGeneratorGraph &graph) {
	if (graph.get_node_type_id(_node_id) == VoxelGeneratorGraph::NODE_EXPRESSION) {
		const String code = graph.get_node_param(_node_id, 0);
		set_title(code);
	}
}

void VoxelGraphEditorNode::update_range_analysis_tooltips(
		const VoxelGeneratorGraph &graph, const VoxelGraphRuntime::State &state) {
	for (unsigned int port_index = 0; port_index < _output_labels.size(); ++port_index) {
		ProgramGraph::PortLocation loc;
		loc.node_id = get_generator_node_id();
		loc.port_index = port_index;
		uint32_t address;
		if (!graph.try_get_output_port_address(loc, address)) {
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

void VoxelGraphEditorNode::set_profiling_ratio_visible(bool visible) {
	if (_profiling_ratio_enabled == visible) {
		return;
	}
	_profiling_ratio_enabled = visible;
	queue_redraw();
}

void VoxelGraphEditorNode::set_profiling_ratio(float ratio) {
	if (_profiling_ratio == ratio) {
		return;
	}
	_profiling_ratio = ratio;
	queue_redraw();
}

// Color has no lerp??
inline Color lerp(Color a, Color b, float t) {
	return Color( //
			Math::lerp(a.r, b.r, t), //
			Math::lerp(a.g, b.g, t), //
			Math::lerp(a.b, b.b, t), //
			Math::lerp(a.a, b.a, t));
}

void VoxelGraphEditorNode::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
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

void VoxelGraphEditorNode::_bind_methods() {
#ifdef ZN_GODOT_EXTENSION
	ClassDB::bind_method(D_METHOD("_on_resize_request", "new_size"), &VoxelGraphEditorNode::_on_resize_request);
#endif
}

} // namespace zylann::voxel
