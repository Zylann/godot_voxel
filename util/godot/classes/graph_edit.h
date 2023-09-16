#ifndef ZN_GODOT_GRAPH_EDIT_H
#define ZN_GODOT_GRAPH_EDIT_H

#if defined(ZN_GODOT)
#include <scene/gui/graph_edit.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/graph_edit.hpp>
using namespace godot;
#endif

#include "../core/string_name.h"
#include "../core/version.h"
#include <vector>

namespace zylann {

struct GodotGraphEditConnection {
	StringName from;
	StringName to;
	int from_port = 0;
	int to_port = 0;
	// float activity = 0.0;
};

void get_graph_edit_connections(const GraphEdit &self, std::vector<GodotGraphEditConnection> &out_connections);
Vector2 get_graph_edit_scroll_offset(const GraphEdit &self);
bool is_graph_edit_using_snapping(const GraphEdit &self);
int get_graph_edit_snapping_distance(const GraphEdit &self);

// Name of the GraphEdit signal to listen to, for the intent of deleting nodes
//
// There was a rename in Godot 4.2 PR #79311 2167694965ca2f4f16cfc1362d32a2fa01e817a2
// https://github.com/godotengine/godot/pull/79311#issuecomment-1671901961
// The original code of GraphEdit referred to deleting GraphNodes as "close" and "delete" interchangeably. It was
// unified for consistency, but the term chosen was "close", which breaks compatibility with a name that is less related
// to what it was used for...
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 1
#define GODOT_GraphEdit_delete_nodes_request "delete_nodes_request"
#else
#define GODOT_GraphEdit_delete_nodes_request "close_nodes_request"
#endif

} // namespace zylann

#endif // ZN_GODOT_GRAPH_EDIT_H
