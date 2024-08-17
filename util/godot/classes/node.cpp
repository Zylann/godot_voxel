#include "node.h"

namespace zylann::godot {

template <typename F>
void for_each_node_depth_first(Node *parent, F f) {
	ERR_FAIL_COND(parent == nullptr);
	f(parent);
	for (int i = 0; i < parent->get_child_count(); ++i) {
		for_each_node_depth_first(parent->get_child(i), f);
	}
}

void set_nodes_owner(Node *root, Node *owner) {
	for_each_node_depth_first(root, [owner](Node *node) { //
		node->set_owner(owner);
	});
}

void set_nodes_owner_except_root(Node *root, Node *owner) {
	ERR_FAIL_COND(root == nullptr);
	for (int i = 0; i < root->get_child_count(); ++i) {
		set_nodes_owner(root->get_child(i), owner);
	}
}

void get_node_groups(const Node &node, StdVector<StringName> &out_groups) {
#if defined(ZN_GODOT)
	List<Node::GroupInfo> gi;
	node.get_groups(&gi);
	for (const Node::GroupInfo &g : gi) {
		out_groups.push_back(g.name);
	}

#elif defined(ZN_GODOT_EXTENSION)
	TypedArray<StringName> groups = node.get_groups();
	for (int i = 0; i < groups.size(); ++i) {
		out_groups.push_back(groups[i]);
	}
#endif
}

} // namespace zylann::godot
