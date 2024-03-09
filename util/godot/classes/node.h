#ifndef ZN_GODOT_NODE_H
#define ZN_GODOT_NODE_H

#if defined(ZN_GODOT)
#include <scene/main/node.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/node.hpp>
using namespace godot;
#endif

#include "../../containers/std_vector.h"
#include "../../errors.h"

namespace zylann::godot {

void set_nodes_owner(Node *root, Node *owner);
void set_nodes_owner_except_root(Node *root, Node *owner);

template <typename T>
inline T *get_node_typed(const Node &self, const NodePath &path) {
#if defined(ZN_GODOT)
	return Object::cast_to<T>(self.get_node(path));
#elif defined(ZN_GODOT_EXTENSION)
	return self.get_node<T>(path);
#endif
}

void get_node_groups(const Node &node, StdVector<StringName> &out_groups);

} // namespace zylann::godot

#endif // ZN_GODOT_NODE_H
