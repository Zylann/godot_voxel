#ifndef ZN_GODOT_GRAPH_NODE_H
#define ZN_GODOT_GRAPH_NODE_H

#if defined(ZN_GODOT)
#include <scene/gui/graph_node.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/graph_node.hpp>
using namespace godot;
#endif

namespace zylann::godot {

Vector2 get_graph_node_input_port_position(GraphNode &node, int port_index);
Vector2 get_graph_node_output_port_position(GraphNode &node, int port_index);
Color get_graph_node_input_port_color(GraphNode &node, int port_index);

} // namespace zylann::godot

#endif // ZN_GODOT_GRAPH_NODE_H
