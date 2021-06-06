#include "voxel_graph_node_inspector_wrapper.h"
#include "../../generators/graph/voxel_graph_node_db.h"
#include "../../util/macros.h"
#include <core/undo_redo.h>

void VoxelGraphNodeInspectorWrapper::setup(Ref<VoxelGeneratorGraph> p_graph, uint32_t p_node_id, UndoRedo *ur) {
	_graph = p_graph;
	_node_id = p_node_id;
	_undo_redo = ur;
}

void VoxelGraphNodeInspectorWrapper::_get_property_list(List<PropertyInfo> *p_list) const {
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND(graph.is_null());

	if (!graph->has_node(_node_id)) {
		// Maybe got erased by the user?
#ifdef DEBUG_ENABLED
		PRINT_VERBOSE("VoxelGeneratorGraph node was not found, from the graph inspector");
#endif
		return;
	}

	p_list->push_back(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_EDITOR));

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);
	const VoxelGraphNodeDB::NodeType &node_type = VoxelGraphNodeDB::get_singleton()->get_type(node_type_id);

	// Params

	p_list->push_back(PropertyInfo(Variant::NIL, "Params", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));

	for (size_t i = 0; i < node_type.params.size(); ++i) {
		const VoxelGraphNodeDB::Param &param = node_type.params[i];
		PropertyInfo pi;
		pi.name = param.name;
		pi.type = param.type;
		pi.class_name = param.class_name;
		if (!param.class_name.empty()) {
			pi.hint = PROPERTY_HINT_RESOURCE_TYPE;
			pi.hint_string = pi.class_name;
		} else if (param.has_range) {
			pi.hint = PROPERTY_HINT_RANGE;
			pi.hint_string = String("{0},{1}").format(varray(param.min_value, param.max_value));
		}
		pi.usage = PROPERTY_USAGE_EDITOR;
		p_list->push_back(pi);
	}

	// Inputs

	p_list->push_back(
			PropertyInfo(Variant::NIL, "Input Defaults", PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));

	for (size_t i = 0; i < node_type.inputs.size(); ++i) {
		const VoxelGraphNodeDB::Port &port = node_type.inputs[i];
		PropertyInfo pi;
		pi.name = port.name;
		pi.type = port.default_value.get_type();
		p_list->push_back(pi);
	}
}

bool VoxelGraphNodeInspectorWrapper::_set(const StringName &p_name, const Variant &p_value) {
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);

	ERR_FAIL_COND_V(_undo_redo == nullptr, false);
	UndoRedo *ur = _undo_redo;

	if (p_name == "name") {
		String previous_name = graph->get_node_name(_node_id);
		ur->create_action("Set VoxelGeneratorGraph node name");
		ur->add_do_method(graph.ptr(), "set_node_name", _node_id, p_value);
		ur->add_undo_method(graph.ptr(), "set_node_name", _node_id, previous_name);
		ur->add_do_method(this, "property_list_changed_notify");
		ur->add_undo_method(this, "property_list_changed_notify");
		ur->commit_action();
		return true;
	}

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);

	uint32_t index;
	if (VoxelGraphNodeDB::get_singleton()->try_get_param_index_from_name(node_type_id, p_name, index)) {
		Variant previous_value = graph->get_node_param(_node_id, index);
		ur->create_action("Set VoxelGeneratorGraph node parameter");
		ur->add_do_method(graph.ptr(), "set_node_param", _node_id, index, p_value);
		ur->add_undo_method(graph.ptr(), "set_node_param", _node_id, index, previous_value);
		ur->add_do_method(this, "property_list_changed_notify");
		ur->add_undo_method(this, "property_list_changed_notify");
		ur->commit_action();

	} else if (VoxelGraphNodeDB::get_singleton()->try_get_input_index_from_name(node_type_id, p_name, index)) {
		Variant previous_value = graph->get_node_default_input(_node_id, index);
		ur->create_action("Set VoxelGeneratorGraph node default input");
		ur->add_do_method(graph.ptr(), "set_node_default_input", _node_id, index, p_value);
		ur->add_undo_method(graph.ptr(), "set_node_default_input", _node_id, index, previous_value);
		ur->add_do_method(this, "property_list_changed_notify");
		ur->add_undo_method(this, "property_list_changed_notify");
		ur->commit_action();

	} else {
		ERR_PRINT(String("Invalid param name {0}").format(varray(p_name)));
		return false;
	}

	return true;
}

bool VoxelGraphNodeInspectorWrapper::_get(const StringName &p_name, Variant &r_ret) const {
	Ref<VoxelGeneratorGraph> graph = get_graph();
	ERR_FAIL_COND_V(graph.is_null(), false);

	if (p_name == "name") {
		r_ret = graph->get_node_name(_node_id);
		return true;
	}

	const uint32_t node_type_id = graph->get_node_type_id(_node_id);

	uint32_t index;
	if (VoxelGraphNodeDB::get_singleton()->try_get_param_index_from_name(node_type_id, p_name, index)) {
		r_ret = graph->get_node_param(_node_id, index);

	} else if (VoxelGraphNodeDB::get_singleton()->try_get_input_index_from_name(node_type_id, p_name, index)) {
		r_ret = graph->get_node_default_input(_node_id, index);

	} else {
		ERR_PRINT(String("Invalid param name {0}").format(varray(p_name)));
		return false;
	}

	return true;
}

// This method is an undocumented hack used in `EditorInspector::_edit_set` so we can implement UndoRedo ourselves.
// If we don't do this, then the inspector's UndoRedo will use the wrapper, which won't mark the real resource as modified.
bool VoxelGraphNodeInspectorWrapper::_dont_undo_redo() const {
	return true;
}

void VoxelGraphNodeInspectorWrapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_dont_undo_redo"), &VoxelGraphNodeInspectorWrapper::_dont_undo_redo);
}
