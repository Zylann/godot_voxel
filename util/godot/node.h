#ifndef ZN_GODOT_NODE_H
#define ZN_GODOT_NODE_H

class Node;

namespace zylann {

void set_nodes_owner(Node *root, Node *owner);
void set_nodes_owner_except_root(Node *root, Node *owner);

} // namespace zylann

#endif // ZN_GODOT_NODE_H
