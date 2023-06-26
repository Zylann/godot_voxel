#ifndef VOXEL_GENERATOR_GRAPH_UNDO_REDO_WORKAROUND_H
#define VOXEL_GENERATOR_GRAPH_UNDO_REDO_WORKAROUND_H

#include "../../generators/graph/voxel_graph_function.h"

namespace zylann::voxel {

class VoxelGeneratorGraph;

// This is a RIDICULOUS workaround for Godot's UndoRedoManager to stop erroring on the fact actions target two resources
// with different automatically-determined-but-wrong undo histories.
//
// UndoRedoManager has multiple histories. One for edited scenes, and a global one.
// When it comes to resources, built-in ones might use per-scene histories while resources saved into a file may use the
// global one.
// When you do:
//
// undo_redo_manager->create_action("Name");
// undo_redo_manager->add_undo_method(resource1, "method1");
// undo_redo_manager->add_undo_method(resource2, "method2");
// undo_redo_manager->commit_action();
//
// UndoRedoManager will pick the history based on the first object (`resource1`). But if later it finds that another
// object has different history (`resource2`), it will error "UndoRedo history mismatch: expected 0, got 2".
// It just doesn't stick to the same history for the whole action.
// So you can't create an action affecting objects with different histories, it seems...
//
// This is a problem for `VoxelGeneratorGraph`. That resource used to be a graph, but later got split in two classes,
// where `VoxelGraphFunction` becomes the graph, and `VoxelGeneratorGraph` just uses it. `VoxelGraphFunction` can exist
// on its own as a re-usable sub-graph, but we wanted to keep compatibility with previous version, while also opening
// `VoxelGeneratorGraph` as if it was still a graph, without the need to click and manage a nested resource. So instead,
// `VoxelGeneratorGraph` ALWAYS comes with an internal `VoxelGraphFunction` object, which is transparently edited by our
// custom editor, the same as with standalone ones.
// The problem is, editing the internal `VoxelGraphFunction` means its containing `VoxelGeneratorGraph` does not appear
// in undo methods, so it never saves. We worked around that by artificially calling a dummy function of
// `VoxelGeneratorGraph` so that it would appear in actions, and get saved. Unfortunately that creates the issue
// mentionned earlier: the action mixes two histories. Because `VoxelGraphFunction` has no path, `UndoRedoManager`
// now assumes it is a "built-in" resource, which leads to different deduced history than the initial
// `VoxelGeneratorGraph`, causing errors.
//
// But this behavior is not desired. We just want Godot to call methods on an object, which we KNOW belongs to the
// resource appearing in the first undo method. But JUST BECAUSE THAT OTHER OBJECT HAPPENS TO BE A RESOURCE, Godot does
// its magic behavior and fails us.
//
// - We don't want to copy all the API from `VoxelGraphFunction` onto `VoxelGeneratorGraph`, it's too messy.
// - We can't get `UndoRedo` history manually from `EditorUndoRedoManager` because according to KoBeWi (Godot dev team)
//   it would cause "history desync". Besides, its API is different and uses `Callable`, which would require to rewrite
//   all our code. It would also not work in GDExtension because `Callable::bind` doesn't work at time of writing
// - We can't give the nested resource a fake path to trick `UndoRedoManager`, because then Godot would try to save it
//   and fail.
// SO...
// This is a class that contains all methods we want to call to edit the graph resource, except it's not a
// `Resource`, so `EditorUndoRedoManager` will not try to deduce the wrong history. We'll attach this to
// `VoxelGeneratorGraph`, and we'll use that in undo actions... We can't even use metadata for doing this because it
// would get saved into the resource file...
class VoxelGeneratorGraphUndoRedoWorkaround : public RefCounted {
	GDCLASS(VoxelGeneratorGraphUndoRedoWorkaround, RefCounted)
public:
	static Ref<VoxelGeneratorGraphUndoRedoWorkaround> get_or_create(Ref<VoxelGeneratorGraph> generator);

private:
	void _b_create_node(pg::VoxelGraphFunction::NodeTypeID type_id, Vector2 position, int node_id);
	void _b_create_function_node(Ref<pg::VoxelGraphFunction> func, Vector2 position, int node_id);
	void _b_remove_node(int node_id);
	void _b_set_node_gui_position(int node_id, Vector2 pos);
	void _b_set_node_gui_size(int node_id, Vector2 size);
	void _b_set_node_name(int node_id, String name);
	void _b_set_node_default_inputs_autoconnect(int node_id, bool value);
	void _b_set_node_param(int node_id, int param_index, Variant value);
	void _b_set_node_default_input(int node_id, int port_index, float value);
	void _b_set_expression_node_inputs(int node_id, PackedStringArray inputs);
	void _b_add_connection(int src_node_id, int src_port_index, int dst_node_id, int dst_port_index);
	void _b_remove_connection(int src_node_id, int src_port_index, int dst_node_id, int dst_port_index);

	static void _bind_methods();

	Ref<pg::VoxelGraphFunction> _graph;
};

} // namespace zylann::voxel

#endif // VOXEL_GENERATOR_GRAPH_UNDO_REDO_WORKAROUND_H
