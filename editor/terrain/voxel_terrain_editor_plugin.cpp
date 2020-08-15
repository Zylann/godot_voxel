#include "voxel_terrain_editor_plugin.h"
#include "../../generators/voxel_generator.h"
#include "../../terrain/voxel_lod_terrain.h"
#include "../../terrain/voxel_terrain.h"
#include "../graph/voxel_graph_node_inspector_wrapper.h"

VoxelTerrainEditorPlugin::VoxelTerrainEditorPlugin(EditorNode *p_node) {
	_restart_stream_button = memnew(Button);
	_restart_stream_button->set_text(TTR("Re-generate"));
	_restart_stream_button->connect("pressed", this, "_on_restart_stream_button_pressed");
	_restart_stream_button->hide();
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, _restart_stream_button);
}

static Node *get_as_terrain(Object *p_object) {
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(p_object);
	if (terrain != nullptr) {
		return terrain;
	}
	VoxelLodTerrain *terrain2 = Object::cast_to<VoxelLodTerrain>(p_object);
	if (terrain2 != nullptr) {
		return terrain2;
	}
	return nullptr;
}

// Things the plugin doesn't directly work on, but still handles to keep things visible.
// This is basically a hack because it's not easy to express that with EditorPlugin API.
// The use case being, as long as we edit an object NESTED within a voxel terrain, we should keep things visible.
static bool is_side_handled(Object *p_object) {
	// Handle stream too so we can leave some controls visible while we edit a stream or generator
	VoxelGenerator *generator = Object::cast_to<VoxelGenerator>(p_object);
	if (generator != nullptr) {
		return true;
	}
	// And have to account for this hack as well
	VoxelGraphNodeInspectorWrapper *wrapper = Object::cast_to<VoxelGraphNodeInspectorWrapper>(p_object);
	if (wrapper != nullptr) {
		return true;
	}
	return false;
}

bool VoxelTerrainEditorPlugin::handles(Object *p_object) const {
	if (get_as_terrain(p_object) != nullptr) {
		return true;
	}
	if (_node != nullptr) {
		return is_side_handled(p_object);
	}
	return false;
}

void VoxelTerrainEditorPlugin::edit(Object *p_object) {
	Node *node = get_as_terrain(p_object);
	if (node != nullptr) {
		set_node(node);
	} else {
		if (!is_side_handled(p_object)) {
			set_node(nullptr);
		}
	}
}

void VoxelTerrainEditorPlugin::set_node(Node *node) {
	if (_node != nullptr) {
		// Using this to know when the node becomes really invalid, because ObjectID is unreliable in Godot 3.x,
		// and we may want to keep access to the node when we select some different kinds of objects.
		// Also moving the node around in the tree triggers exit/enter so have to listen for both.
		_node->disconnect("tree_entered", this, "_on_terrain_tree_entered");
		_node->disconnect("tree_exited", this, "_on_terrain_tree_exited");
	}

	_node = node;

	if (_node != nullptr) {
		_node->connect("tree_entered", this, "_on_terrain_tree_entered", varray(_node));
		_node->connect("tree_exited", this, "_on_terrain_tree_exited", varray(_node));
	}
}

void VoxelTerrainEditorPlugin::make_visible(bool visible) {
	_restart_stream_button->set_visible(visible);
	// Can't use `make_visible(false)` to reset our reference to the node,
	// because of https://github.com/godotengine/godot/issues/40166
	// So we'll need to check if _node is null all over the place
}

void VoxelTerrainEditorPlugin::_on_restart_stream_button_pressed() {
	ERR_FAIL_COND(_node == nullptr);
	VoxelTerrain *terrain = Object::cast_to<VoxelTerrain>(_node);
	if (terrain != nullptr) {
		terrain->restart_stream();
		return;
	}
	VoxelLodTerrain *terrain2 = Object::cast_to<VoxelLodTerrain>(_node);
	ERR_FAIL_COND(terrain2 == nullptr);
	terrain2->restart_stream();
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_entered(Node *node) {
	// If the node exited the tree because it was deleted, signals we connected should automatically disconnect.
	_node = node;
}

void VoxelTerrainEditorPlugin::_on_terrain_tree_exited(Node *node) {
	_node = nullptr;
}

void VoxelTerrainEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_restart_stream_button_pressed"),
			&VoxelTerrainEditorPlugin::_on_restart_stream_button_pressed);
	ClassDB::bind_method(D_METHOD("_on_terrain_tree_entered"), &VoxelTerrainEditorPlugin::_on_terrain_tree_entered);
	ClassDB::bind_method(D_METHOD("_on_terrain_tree_exited"), &VoxelTerrainEditorPlugin::_on_terrain_tree_exited);
}
