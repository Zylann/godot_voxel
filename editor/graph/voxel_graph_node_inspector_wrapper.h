#ifndef VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H
#define VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H

#include "../../generators/graph/voxel_generator_graph.h"
#include "../../util/godot/classes/ref_counted.h"

namespace zylann::voxel {

class VoxelGraphEditor;

// Nodes aren't resources so this translates them into a form the inspector can understand.
// This makes it easier to support undo/redo and sub-resources.
// WARNING: `AnimationPlayer` will allow to keyframe properties, but there really is no support for that.
class VoxelGraphNodeInspectorWrapper : public RefCounted {
	GDCLASS(VoxelGraphNodeInspectorWrapper, RefCounted)
public:
	void setup(uint32_t p_node_id, VoxelGraphEditor *ed);

	// May be called when the graph editor is destroyed. This prevents from accessing dangling pointers in the eventual
	// case where UndoRedo invokes functions from this editor after the plugin is removed.
	void detach_from_graph_editor();

	inline Ref<pg::VoxelGraphFunction> get_graph() const {
		return _graph;
	}
	inline Ref<VoxelGeneratorGraph> get_generator() const {
		return _generator;
	}

protected:
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _dont_undo_redo() const;

private:
	static void _bind_methods();

	Ref<pg::VoxelGraphFunction> _graph;
	Ref<VoxelGeneratorGraph> _generator;
	uint32_t _node_id = ProgramGraph::NULL_ID;
	VoxelGraphEditor *_graph_editor = nullptr;
};

} // namespace zylann::voxel

#endif // VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H
