#ifndef ZN_GODOT_GRAPH_NODE_H
#define ZN_GODOT_GRAPH_NODE_H

#if defined(ZN_GODOT)
#include <scene/gui/graph_node.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/graph_node.hpp>
using namespace godot;
#endif

#endif // ZN_GODOT_GRAPH_NODE_H
