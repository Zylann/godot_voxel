#include "voxel_generator_graph_undo_redo_workaround.h"
#include "../../generators/graph/voxel_generator_graph.h"

namespace zylann::voxel {

Ref<VoxelGeneratorGraphUndoRedoWorkaround> VoxelGeneratorGraphUndoRedoWorkaround::get_or_create(
		Ref<VoxelGeneratorGraph> generator) {
	ZN_ASSERT(generator.is_valid());
	Ref<VoxelGeneratorGraphUndoRedoWorkaround> w = generator->editor_undo_redo_workaround;
	if (w.is_null()) {
		w.instantiate();
		generator->editor_undo_redo_workaround = w;
	}
	w->_graph = generator->get_main_function();
	return w;
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_create_node(
		pg::VoxelGraphFunction::NodeTypeID type_id, Vector2 position, int node_id) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->create_node(type_id, position, node_id);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_create_function_node(
		Ref<pg::VoxelGraphFunction> func, Vector2 position, int node_id) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->create_function_node(func, position, node_id);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_remove_node(int node_id) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->remove_node(node_id);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_gui_position(int node_id, Vector2 pos) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_gui_position(node_id, pos);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_gui_size(int node_id, Vector2 size) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_gui_size(node_id, size);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_name(int node_id, String name) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_name(node_id, name);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_default_inputs_autoconnect(int node_id, bool value) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_default_inputs_autoconnect(node_id, value);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_param(int node_id, int param_index, Variant value) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_param(node_id, param_index, value);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_default_input(int node_id, int port_index, float value) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_node_default_input(node_id, port_index, value);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_set_expression_node_inputs(int node_id, PackedStringArray inputs) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->set_expression_node_inputs(node_id, inputs);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_add_connection(
		int src_node_id, int src_port_index, int dst_node_id, int dst_port_index) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->add_connection(src_node_id, src_port_index, dst_node_id, dst_port_index);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_b_remove_connection(
		int src_node_id, int src_port_index, int dst_node_id, int dst_port_index) {
	ZN_ASSERT_RETURN(_graph.is_valid());
	_graph->remove_connection(src_node_id, src_port_index, dst_node_id, dst_port_index);
}

void VoxelGeneratorGraphUndoRedoWorkaround::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_node", "type_id", "position", "id"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_create_node, DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("create_function_node", "position", "id"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_create_function_node, DEFVAL(ProgramGraph::NULL_ID));
	ClassDB::bind_method(D_METHOD("remove_node", "node_id"), &VoxelGeneratorGraphUndoRedoWorkaround::_b_remove_node);
	ClassDB::bind_method(D_METHOD("set_node_gui_position", "node_id", "position"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_gui_position);
	ClassDB::bind_method(D_METHOD("set_node_gui_size", "node_id", "size"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_gui_size);
	ClassDB::bind_method(
			D_METHOD("set_node_name", "node_id", "name"), &VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_name);
	ClassDB::bind_method(D_METHOD("set_node_default_inputs_autoconnect", "node_id", "autoconnect_enabled"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_default_inputs_autoconnect);
	ClassDB::bind_method(D_METHOD("set_node_param", "node_id", "param_index", "value"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_param);
	ClassDB::bind_method(D_METHOD("set_node_default_input", "node_id", "port_index", "value"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_node_default_input);
	ClassDB::bind_method(D_METHOD("set_expression_node_inputs", "node_id", "inputs"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_set_expression_node_inputs);
	ClassDB::bind_method(D_METHOD("add_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_add_connection);
	ClassDB::bind_method(
			D_METHOD("remove_connection", "src_node_id", "src_port_index", "dst_node_id", "dst_port_index"),
			&VoxelGeneratorGraphUndoRedoWorkaround::_b_remove_connection);
}

} // namespace zylann::voxel
