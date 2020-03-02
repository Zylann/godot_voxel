#ifndef VOXEL_GRAPH_EDITOR_H
#define VOXEL_GRAPH_EDITOR_H

#include <scene/gui/control.h>

class VoxelGeneratorGraph;
class GraphEdit;
class PopupMenu;

class VoxelGraphEditor : public Control {
	GDCLASS(VoxelGraphEditor, Control)
public:
	VoxelGraphEditor();

	void set_graph(Ref<VoxelGeneratorGraph> graph);

private:
	void clear();
	void build_gui_from_graph();
	void build_gui_from_node(uint32_t node_id);

	void _on_graph_edit_gui_input(Ref<InputEvent> event);
	void _on_graph_edit_connection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_delete_nodes_request();
	void _on_context_menu_index_pressed(int idx);

	static void _bind_methods();

	Ref<VoxelGeneratorGraph> _graph;
	GraphEdit *_graph_edit = nullptr;
	PopupMenu *_context_menu = nullptr;
	Vector2 _click_position;
};

#endif // VOXEL_GRAPH_EDITOR_H
