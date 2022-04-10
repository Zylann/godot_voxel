#ifndef VOXEL_GRAPH_EDITOR_NODE_H
#define VOXEL_GRAPH_EDITOR_NODE_H

#include <scene/gui/graph_node.h>
#include <vector>

#include "../../generators/graph/voxel_graph_runtime.h"

class ColorRect;

namespace zylann::voxel {

class VoxelGraphEditorNodePreview;
class VoxelGeneratorGraph;

// GUI graph node with a few custom data attached.
class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	static VoxelGraphEditorNode *create(const VoxelGeneratorGraph &graph, uint32_t node_id);

	void update_title(StringName node_name, String node_type_name);
	void poll(const VoxelGeneratorGraph &graph);

	void update_range_analysis_tooltips(const VoxelGeneratorGraph &graph, const VoxelGraphRuntime::State &state);
	void clear_range_analysis_tooltips();

	void update_layout(const VoxelGeneratorGraph &graph);

	bool has_outputs() const {
		return _output_labels.size() > 0;
	}

	inline uint32_t get_generator_node_id() const {
		return _node_id;
	}

	inline VoxelGraphEditorNodePreview *get_preview() const {
		return _preview;
	}

	void set_profiling_ratio_visible(bool visible);
	void set_profiling_ratio(float ratio);

private:
	void poll_default_inputs(const VoxelGeneratorGraph &graph);
	void poll_params(const VoxelGeneratorGraph &graph);
	void _on_resize_request(Vector2 new_size);

	void _notification(int p_what);

	uint32_t _node_id = 0;
	VoxelGraphEditorNodePreview *_preview = nullptr;
	std::vector<Control *> _output_labels;

	struct InputHint {
		Label *label;
		Variant last_value;
	};

	std::vector<InputHint> _input_hints;
	std::vector<Node *> _rows;

	float _profiling_ratio = 0.f;
	bool _profiling_ratio_enabled = false;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_H
