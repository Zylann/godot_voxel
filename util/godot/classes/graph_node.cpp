#include "graph_node.h"
#include "../core/version.h"

namespace zylann {

// There were changes to GraphNode in Godot PR #79311 2167694965ca2f4f16cfc1362d32a2fa01e817a2

// For some reason these getters cannot be const...

Vector2 get_graph_node_input_port_position(GraphNode &node, int port_index) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 1
	// Can't directly use inputs and output positions... Godot pre-scales them, which makes them unusable
	// inside NOTIFICATION_DRAW because the node is already scaled
	const Vector2 scale = node.get_global_transform().get_scale();
	return node.get_connection_input_position(port_index) / scale;
#else
	return node.get_input_port_position(port_index);
#endif
}

Vector2 get_graph_node_output_port_position(GraphNode &node, int port_index) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 1
	const Vector2 scale = node.get_global_transform().get_scale();
	return node.get_connection_output_position(port_index) / scale;
#else
	return node.get_output_port_position(port_index);
#endif
}

Color get_graph_node_input_port_color(GraphNode &node, int port_index) {
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR <= 1
	return node.get_connection_input_color(port_index);
#else
	return node.get_input_port_color(port_index);
#endif
}

} // namespace zylann
