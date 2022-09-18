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

// TODO GDX: `NOTIFICATION_*` (and maybe other constants) are not static, they are supposed to be! Can't be use them in
// `switch`, and they take space in every class instance.
namespace godot_cpp_fix {
#ifdef ZN_GODOT_EXTENSION
#define ZN_GODOT_NODE_CONSTANT(m_name) godot_cpp_fix::m_name
static const int NOTIFICATION_PREDELETE = 1;
static const int NOTIFICATION_ENTER_TREE = 10;
static const int NOTIFICATION_EXIT_TREE = 11;
static const int NOTIFICATION_PHYSICS_PROCESS = 16;
static const int NOTIFICATION_PROCESS = 17;
static const int NOTIFICATION_PARENTED = 18;
static const int NOTIFICATION_UNPARENTED = 19;
static const int NOTIFICATION_INTERNAL_PROCESS = 25;
#else
#define ZN_GODOT_NODE_CONSTANT(m_name) m_name
#endif // ZN_GODOT_EXTENSION
} // namespace godot_cpp_fix

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
