#include "voxel_graph_editor_plugin.h"
#include "../generators/graph/voxel_generator_graph.h"
#include "editor/editor_scale.h"
#include "voxel_graph_editor.h"

VoxelGraphEditorPlugin::VoxelGraphEditorPlugin(EditorNode *p_node) {
	//EditorInterface *ed = get_editor_interface();
	_graph_editor = memnew(VoxelGraphEditor);
	_graph_editor->set_custom_minimum_size(Size2(0, 300) * EDSCALE);
	_bottom_panel_button = add_control_to_bottom_panel(_graph_editor, TTR("Voxel Graph"));
	_bottom_panel_button->hide();
}

bool VoxelGraphEditorPlugin::handles(Object *p_object) const {
	if (p_object == nullptr) {
		return false;
	}
	VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	return graph_ptr != nullptr;
}

void VoxelGraphEditorPlugin::edit(Object *p_object) {
	VoxelGeneratorGraph *graph_ptr = Object::cast_to<VoxelGeneratorGraph>(p_object);
	Ref<VoxelGeneratorGraph> graph(graph_ptr);
	_graph_editor->set_undo_redo(&get_undo_redo()); // UndoRedo isn't available in constructor
	_graph_editor->set_graph(graph);
}

void VoxelGraphEditorPlugin::make_visible(bool visible) {
	if (visible) {
		_bottom_panel_button->show();
		make_bottom_panel_item_visible(_graph_editor);
	} else {
		_bottom_panel_button->hide();
		edit(nullptr);
		if (_graph_editor->is_visible_in_tree()) {
			hide_bottom_panel();
		}
	}
}
