#ifndef VOXEL_GRAPH_EDITOR_NODE_H
#define VOXEL_GRAPH_EDITOR_NODE_H

#include <scene/gui/graph_node.h>
#include <vector>

namespace zylann::voxel {

class VoxelGraphEditorNodePreview;

// Graph node with a few custom data attached.
class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	uint32_t node_id = 0;
	VoxelGraphEditorNodePreview *preview = nullptr;
	Vector<Control *> output_labels;

	struct InputHint {
		Label *label;
		Variant last_value;
	};

	std::vector<InputHint> input_hints;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_H
