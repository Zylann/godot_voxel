#ifndef VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H
#define VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H

#include "../../generators/graph/voxel_generator_graph.h"
#include <core/reference.h>

class UndoRedo;

// Nodes aren't resources so this translates them into a form the inspector can understand.
// This makes it easier to support undo/redo and sub-resources.
// WARNING: `AnimationPlayer` will allow to keyframe properties, but there really is no support for that.
class VoxelGraphNodeInspectorWrapper : public Reference {
	GDCLASS(VoxelGraphNodeInspectorWrapper, Reference)
public:
	void setup(Ref<VoxelGeneratorGraph> p_graph, uint32_t p_node_id, UndoRedo *ur);
	inline Ref<VoxelGeneratorGraph> get_graph() const { return _graph; }

protected:
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _dont_undo_redo() const;

private:
	static void _bind_methods();

	Ref<VoxelGeneratorGraph> _graph;
	uint32_t _node_id = ProgramGraph::NULL_ID;
	UndoRedo *_undo_redo = nullptr;
};

#endif // VOXEL_GRAPH_NODE_INSPECTOR_WRAPPER_H
