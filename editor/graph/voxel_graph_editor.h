#ifndef VOXEL_GRAPH_EDITOR_H
#define VOXEL_GRAPH_EDITOR_H

#include "../../generators/graph/voxel_generator_graph.h"
#include "../../util/godot/classes/control.h"
#include "../../util/godot/classes/editor_undo_redo_manager.h"
#include "../../util/math/vector2f.h"
#include "../voxel_debug.h"

ZN_GODOT_FORWARD_DECLARE(class GraphEdit)
ZN_GODOT_FORWARD_DECLARE(class PopupMenu)
ZN_GODOT_FORWARD_DECLARE(class AcceptDialog)
ZN_GODOT_FORWARD_DECLARE(class UndoRedo)
ZN_GODOT_FORWARD_DECLARE(class Button)
ZN_GODOT_FORWARD_DECLARE(class Label)
ZN_GODOT_FORWARD_DECLARE(class OptionButton)
ZN_GODOT_FORWARD_DECLARE(class CheckBox)
ZN_GODOT_FORWARD_DECLARE(class EditorFileDialog)
ZN_GODOT_FORWARD_DECLARE(class MenuButton)
#ifdef ZN_GODOT
ZN_GODOT_FORWARD_DECLARE(class EditorQuickOpen)
#endif

namespace zylann::voxel {

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
	static const char *SIGNAL_REGENERATE_REQUESTED;
	static const char *SIGNAL_POPOUT_REQUESTED;

	VoxelGraphEditor();

	void set_generator(Ref<VoxelGeneratorGraph> generator);
	inline Ref<VoxelGeneratorGraph> get_generator() const {
		return _generator;
	}

	void set_graph(Ref<pg::VoxelGraphFunction> graph);
	Ref<pg::VoxelGraphFunction> get_graph() const;

	void set_undo_redo(EditorUndoRedoManager *undo_redo);
	EditorUndoRedoManager *get_undo_redo() const;

	void set_voxel_node(VoxelNode *node);

	// To be called when the number of inputs in a node changes.
	// Rebuilds the node's internal controls, and updates GUI connections going to it from the graph.
	void update_node_layout(uint32_t node_id);

	void update_node_comment(uint32_t node_id);

	bool is_pinned_hint() const;
	void set_popout_button_enabled(bool enable);

#ifdef ZN_GODOT_EXTENSION
	void _process(double delta) override;
#endif

private:
	void _notification(int p_what);
	void process(float delta);

	void clear();
	void build_gui_from_graph();
	void create_node_gui(uint32_t node_id);
	void remove_node_gui(StringName gui_node_name);
	void set_node_position(int id, Vector2 offset);
	void set_node_size(int id, Vector2 size);

	void schedule_preview_update();
	void update_previews(bool with_live_update);
	void update_slice_previews();
	void update_range_analysis_previews();
	void update_range_analysis_gizmo();
	void clear_range_analysis_tooltips();
	void hide_profiling_ratios();
	void update_buttons_availability();
	void create_function_node(String fpath);
	void profile();
	void update_preview_axes_menu();
	void update_functions();
	void set_preview_transform(Vector2f offset, float scale);

	void _on_graph_edit_gui_input(Ref<InputEvent> event);
	void _on_graph_edit_connection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);
	void _on_graph_edit_disconnection_request(String from_node_name, int from_slot, String to_node_name, int to_slot);

#if defined(ZN_GODOT)
	void _on_graph_edit_delete_nodes_request(TypedArray<StringName> node_names);
	void _on_graph_edit_node_selected(Node *p_node);
	void _on_graph_edit_node_deselected(Node *p_node);
#elif defined(ZN_GODOT_EXTENSION)
	// TODO GDX: TypedArray isn't available.
	void _on_graph_edit_delete_nodes_request(Array node_names);
	// TODO GDX: Can't bind methods taking a child class of `Object*`
	void _on_graph_edit_node_selected(Object *p_node_o);
	void _on_graph_edit_node_deselected(Object *p_node_o);
#endif

	void _on_menu_id_pressed(int id);
	void _on_graph_node_dragged(Vector2 from, Vector2 to, int id);
	void _on_context_menu_id_pressed(int id);
	void _on_graph_changed();
	void _on_graph_node_name_changed(int node_id);
	void _on_range_analysis_toggled(bool enabled);
	void _on_range_analysis_area_changed();
	void _on_popout_button_pressed();
	void _on_function_file_dialog_file_selected(String fpath);
#ifdef ZN_GODOT
	void _on_function_quick_open_dialog_quick_open();
#endif
	void _on_node_resize_request(Vector2 new_size, int node_id);
	void _on_graph_node_preview_gui_input(Ref<InputEvent> event);

	void _check_nothing_selected();

	static void _bind_methods();

	Ref<VoxelGeneratorGraph> _generator;
	Ref<pg::VoxelGraphFunction> _graph;

	GraphEdit *_graph_edit = nullptr;
	PopupMenu *_context_menu = nullptr;
	Label *_profile_label = nullptr;
	Label *_compile_result_label = nullptr;
	VoxelRangeAnalysisDialog *_range_analysis_dialog = nullptr;
	EditorFileDialog *_function_file_dialog = nullptr;
#ifdef ZN_GODOT
	// TODO GDX: EditorQuickOpen is not exposed!
	EditorQuickOpen *_function_quick_open_dialog = nullptr;
#endif
	// Not owned.
	// TODO Not sure if using `EditorUndoRedoManager` directly is the right thing to do?
	// VisualShader did it that way when this manager got introduced in place of the old global UndoRedo...
	// there doesn't seem to be any documentation yet for this class
	EditorUndoRedoManager *_undo_redo = nullptr;
	Vector2 _click_position;
	bool _nothing_selected_check_scheduled = false;
	float _time_before_preview_update = 0.f;
	VoxelNode *_voxel_node = nullptr;
	DebugRenderer _debug_renderer;
	VoxelGraphEditorShaderDialog *_shader_dialog = nullptr;
	bool _live_update_enabled = false;
	uint64_t _last_output_graph_hash = 0;
	Button *_pin_button = nullptr;
	Button *_popout_button = nullptr;
	MenuButton *_graph_menu_button = nullptr;
	MenuButton *_debug_menu_button = nullptr;
	PopupMenu *_preview_axes_menu = nullptr;

	enum PreviewAxes { //
		PREVIEW_AXES_XY = 0,
		PREVIEW_AXES_XZ,
		PREVIEW_AXES_OPTIONS_COUNT
	};

	PreviewAxes _preview_axes = PREVIEW_AXES_XY;
	Vector2f _preview_offset;
	float _preview_scale = 1.f;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_H
