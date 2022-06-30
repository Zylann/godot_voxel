#ifndef VOXEL_GRAPH_EDITOR_H
#define VOXEL_GRAPH_EDITOR_H

#include "../voxel_debug.h"
#include <scene/gui/control.h>

class GraphEdit;
class PopupMenu;
class AcceptDialog;
class UndoRedo;

namespace zylann::voxel {

class VoxelGeneratorGraph;
class VoxelRangeAnalysisDialog;
class VoxelNode;
class VoxelGraphEditorShaderDialog;

// Main GUI of the graph editor
class VoxelGraphEditor : public Control {
	GDCLASS(VoxelGraphEditor, Control)
public:
	static const char *SIGNAL_NODE_SELECTED;
	static const char *SIGNAL_NOTHING_SELECTED;
	static const char *SIGNAL_NODES_DELETED;

	VoxelGraphEditor();

	void set_graph(Ref<VoxelGeneratorGraph> graph);
	inline Ref<VoxelGeneratorGraph> get_graph() const {
		return _graph;
	}

	void set_undo_redo(UndoRedo *undo_redo);
	void set_voxel_node(VoxelNode *node);

	// To be called when the number of inputs in a node changes.
	// Rebuilds the node's internal controls, and updates GUI connections going to it from the graph.
	void update_node_layout(uint32_t node_id);

private:
	void _notification(int p_what);
	void _process(float delta);

	void clear();
	void build_gui_from_graph();
	void create_node_gui(uint32_t node_id);
	void remove_node_gui(StringName gui_node_name);
	void set_node_position(int id, Vector2 offset);

	void schedule_preview_update();
	void update_previews(bool with_live_update);
	void update_slice_previews();
	void update_range_analysis_previews();
	void update_range_analysis_gizmo();
	void clear_range_analysis_tooltips();
	void hide_profiling_ratios();

	void _on_graph_edit_gui_input(Ref<InputEvent> event);
	void _on_graph_edit_connection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_delete_nodes_request(TypedArray<StringName> node_names);
	void _on_graph_edit_node_selected(Node *p_node);
	void _on_graph_edit_node_deselected(Node *p_node);
	void _on_graph_node_dragged(Vector2 from, Vector2 to, int id);
	void _on_context_menu_id_pressed(int id);
	void _on_update_previews_button_pressed();
	void _on_profile_button_pressed();
	void _on_graph_changed();
	void _on_graph_node_name_changed(int node_id);
	void _on_analyze_range_button_pressed();
	void _on_range_analysis_toggled(bool enabled);
	void _on_range_analysis_area_changed();
	void _on_preview_axes_menu_id_pressed(int id);
	void _on_generate_shader_button_pressed();
	void _on_live_update_toggled(bool enabled);

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
	VoxelNode *_voxel_node = nullptr;
	DebugRenderer _debug_renderer;
	VoxelGraphEditorShaderDialog *_shader_dialog = nullptr;
	bool _live_update_enabled = false;
	uint64_t _last_output_graph_hash = 0;

	enum PreviewAxes { //
		PREVIEW_XY = 0,
		PREVIEW_XZ,
		PREVIEW_AXES_OPTIONS_COUNT
	};

	PreviewAxes _preview_axes = PREVIEW_XY;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_H
