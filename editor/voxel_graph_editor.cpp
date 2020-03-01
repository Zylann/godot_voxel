#include "voxel_graph_editor.h"
#include "../generators/graph/voxel_generator_graph.h"
#include <scene/gui/graph_edit.h>

VoxelGraphEditor::VoxelGraphEditor() {
	_graph_edit = memnew(GraphEdit);
	_graph_edit->set_anchors_preset(Control::PRESET_WIDE);
	add_child(_graph_edit);
}

void VoxelGraphEditor::set_graph(Ref<VoxelGeneratorGraph> graph) {
	if (_graph == graph) {
		return;
	}

	//	if (_graph.is_valid()) {
	//	}

	_graph = graph;

	//	if (_graph.is_valid()) {
	//	}
}

void VoxelGraphEditor::_bind_methods() {
}
