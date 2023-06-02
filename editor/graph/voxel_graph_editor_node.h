#ifndef VOXEL_GRAPH_EDITOR_NODE_H
#define VOXEL_GRAPH_EDITOR_NODE_H

#include <vector>

#include "../../generators/graph/voxel_graph_runtime.h"
#include "../../util/godot/classes/graph_node.h"

ZN_GODOT_FORWARD_DECLARE(class ColorRect)
ZN_GODOT_FORWARD_DECLARE(class Label)

namespace zylann::voxel {

class VoxelGraphEditorNodePreview;
class VoxelGeneratorGraph;
namespace pg {
class VoxelGraphFunction;
}

// GUI graph node with a few custom data attached.
class VoxelGraphEditorNode : public GraphNode {
	GDCLASS(VoxelGraphEditorNode, GraphNode)
public:
	static VoxelGraphEditorNode *create(const pg::VoxelGraphFunction &graph, uint32_t node_id);

	void update_title(const pg::VoxelGraphFunction &graph);
	void poll(const pg::VoxelGraphFunction &graph);

	void update_range_analysis_tooltips(const VoxelGeneratorGraph &generator, const pg::Runtime::State &state);
	void clear_range_analysis_tooltips();

	void update_layout(const pg::VoxelGraphFunction &graph);
	void update_comment_text(const pg::VoxelGraphFunction &graph);

	bool has_outputs() const {
		return _output_labels.size() > 0;
	}

	inline uint32_t get_generator_node_id() const {
		return _node_id;
	}

	inline VoxelGraphEditorNodePreview *get_preview() const {
		return _preview;
	}

	void set_profiling_ratio_visible(bool p_visible);
	void set_profiling_ratio(float ratio);

private:
	void update_title(const pg::VoxelGraphFunction &graph, uint32_t node_id);
	void poll_default_inputs(const pg::VoxelGraphFunction &graph);
	void poll_params(const pg::VoxelGraphFunction &graph);

	void _notification(int p_what);

	static void _bind_methods();

	uint32_t _node_id = 0;
	VoxelGraphEditorNodePreview *_preview = nullptr;
	std::vector<Control *> _output_labels;
	Label *_comment_label = nullptr;

	struct InputHint {
		Label *label;
		Variant last_value;
	};

	std::vector<InputHint> _input_hints;
	std::vector<Node *> _rows;

	float _profiling_ratio = 0.f;
	bool _profiling_ratio_enabled = false;
	bool _is_relay = false;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_EDITOR_NODE_H
