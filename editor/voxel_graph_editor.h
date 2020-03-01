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
	static void _bind_methods();

	Ref<VoxelGeneratorGraph> _graph;
	GraphEdit *_graph_edit = nullptr;
	PopupMenu *_context_menu = nullptr;
};

#endif // VOXEL_GRAPH_EDITOR_H
