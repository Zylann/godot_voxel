#ifndef VOXEL_GRAPH_EDITOR_H
#define VOXEL_GRAPH_EDITOR_H

#include "../voxel_debug.h"
#include <scene/gui/control.h>

class VoxelGeneratorGraph;
class GraphEdit;
class PopupMenu;
class AcceptDialog;
class UndoRedo;
class VoxelRangeAnalysisDialog;
class VoxelNode;
class Spatial;

class VoxelGraphEditor : public Control {
	GDCLASS(VoxelGraphEditor, Control)
public:
	static const char *SIGNAL_NODE_SELECTED;
	static const char *SIGNAL_NOTHING_SELECTED;

	VoxelGraphEditor();

	void set_graph(Ref<VoxelGeneratorGraph> graph);
	inline Ref<VoxelGeneratorGraph> get_graph() const { return _graph; }

	void set_undo_redo(UndoRedo *undo_redo);
	void set_voxel_node(VoxelNode *node);

private:
	void _notification(int p_what);
	void _process(float delta);

	void clear();
	void build_gui_from_graph();
	void create_node_gui(uint32_t node_id);
	void remove_node_gui(StringName gui_node_name);
	void set_node_position(int id, Vector2 offset);

	void schedule_preview_update();
	void update_previews();
	void update_slice_previews();
	void update_range_analysis_previews();
	void update_range_analysis_gizmo();
	void clear_range_analysis_tooltips();

	void _on_graph_edit_gui_input(Ref<InputEvent> event);
	void _on_graph_edit_connection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_delete_nodes_request(Array nodes);
	void _on_graph_edit_node_selected(Node *p_node);
	void _on_graph_edit_node_unselected(Node *p_node);
	void _on_graph_node_dragged(Vector2 from, Vector2 to, int id);
	void _on_context_menu_id_pressed(int id);
	void _on_update_previews_button_pressed();
	void _on_profile_button_pressed();
	void _on_graph_changed();
	void _on_graph_node_name_changed(int node_id);
	void _on_analyze_range_button_pressed();
	void _on_range_analysis_toggled(bool enabled);
	void _on_range_analysis_area_changed();

	void _check_nothing_selected();

	static void _bind_methods();

	Ref<VoxelGeneratorGraph> _graph;

	GraphEdit *_graph_edit = nullptr;
	PopupMenu *_context_menu = nullptr;
	Label *_profile_label = nullptr;
	Label *_compile_result_label = nullptr;
	VoxelRangeAnalysisDialog *_range_analysis_dialog = nullptr;
	UndoRedo *_undo_redo = nullptr;
	Vector2 _click_position;
	bool _nothing_selected_check_scheduled = false;
	float _time_before_preview_update = 0.f;
	Spatial *_voxel_node = nullptr;
	VoxelDebug::DebugRenderer _debug_renderer;
};

#endif // VOXEL_GRAPH_EDITOR_H
