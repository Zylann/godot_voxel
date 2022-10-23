#ifndef ZN_GODOT_NODE_H
#define ZN_GODOT_NODE_H

#if defined(ZN_GODOT)
#include <scene/main/node.h>
#elif defined(ZN_GODOT_EXTENSION)
#include <godot_cpp/classes/node.hpp>
using namespace godot;
#endif

#include "../errors.h"

namespace zylann {

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

inline void queue_free_node(Node *node) {
#ifdef DEBUG_ENABLED
	ZN_ASSERT_RETURN(node != nullptr);
#endif
#if defined(ZN_GODOT)
	node->queue_delete();
#elif defined(ZN_GODOT_EXTENSION)
	node->queue_free();
#endif
}

} // namespace zylann

#endif // ZN_GODOT_NODE_H
